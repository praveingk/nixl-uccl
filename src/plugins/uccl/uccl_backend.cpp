/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "uccl_backend.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

// Helper function to parse connection string in format: ip_addr:port?gpu_index
bool parseConnectionString(const std::string& conn_str, char*& ip_addr, int& port, int& gpu_index) {
    // Exit with errror if neither : or ? is found in conn_str
    size_t colon_pos = conn_str.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid connection string format: missing colon separator" << std::endl;
        return false;
    }
    size_t question_pos = conn_str.find('?', colon_pos);
    if (question_pos == std::string::npos) {
        std::cerr << "Invalid connection string format: missing question mark separator" << std::endl;
        return false;
    }

    std::string ip_str = conn_str.substr(0, colon_pos);
    ip_addr = new char[ip_str.length() + 1];
    strcpy(ip_addr, ip_str.c_str());

    std::string port_str = conn_str.substr(colon_pos + 1, question_pos - colon_pos - 1);
    try {
        port = std::stoi(port_str);
    } catch (const std::exception& e) {
        std::cerr << "Invalid port number: " << port_str << std::endl;
        delete[] ip_addr;
        return false;
    }

    std::string gpu_str = conn_str.substr(question_pos + 1);
    try {
        gpu_index = std::stoi(gpu_str);
    } catch (const std::exception& e) {
        std::cerr << "Invalid GPU index: " << gpu_str << std::endl;
        delete[] ip_addr;
        return false;
    }

    return true;
}

nixlUcclEngine::nixlUcclEngine(const nixlBackendInitParams* init_params)
    : nixlBackendEngine(init_params) {
    local_agent_name_ = init_params->localAgent;
    // TODO: Initialize UCCL engine with appropriate GPU index and CPU count
    // For now, use dummy values (0, 1). But extend it to all devices
    engine_ = uccl_engine_create(0, 1);
    std::cout << "UCCL engine created" << std::endl;
}

nixlUcclEngine::~nixlUcclEngine() {
    for (auto& [agent_name, conn_id] : connected_agents_) {
        uccl_conn_t* conn = reinterpret_cast<uccl_conn_t*>(conn_id);
        if (conn) {
            std::cout << "Disconnecting from agent: " << agent_name << std::endl;
            uccl_engine_conn_destroy(conn);
        }
    }
    
    connected_agents_.clear();
    if (engine_) {
        uccl_engine_destroy(engine_);
        engine_ = nullptr;
    }
}

nixl_mem_list_t nixlUcclEngine::getSupportedMems() const {
    nixl_mem_list_t mems;
    mems.push_back(DRAM_SEG);
    mems.push_back(VRAM_SEG);
    return mems;
}

nixl_status_t nixlUcclEngine::getPublicData(const nixlBackendMD* meta, std::string &str) const {
    // UCCL does not expose public metadata for memory regions
    str.clear();
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::getConnInfo(std::string &str) const {
    if (!engine_) {
        return NIXL_ERR_BACKEND;
    }

    char* metadata = nullptr;
    int result = uccl_engine_get_metadata(engine_, &metadata);
    if (result != 0 || !metadata) {
        return NIXL_ERR_BACKEND;
    }

    str = std::string(metadata);
    delete[] metadata;
    std::cout << "UCCL engine metadata: " << str << std::endl;
    return NIXL_SUCCESS;
}
#define MAX_RETRIES 100

nixl_status_t nixlUcclEngine::loadRemoteConnInfo(const std::string &remote_agent, const std::string &remote_conn_info) {
    // Parse remote_conn_info and establish connection using UCCL engine
    std::cout << "UCCL engine remote_agent: "<<remote_agent<<" loadRemoteConnInfo: " << remote_conn_info << std::endl;
    std::lock_guard<std::mutex> lock(mutex_);

    char* ip_addr = nullptr;
    int port = 0;
    int gpu_index = 0;

    if (!parseConnectionString(remote_conn_info, ip_addr, port, gpu_index)) {
        return NIXL_ERR_BACKEND;
    }

    // Simple role coordination: agent with smaller name acts as client
    is_client_ = local_agent_name_ < remote_agent;
    uccl_conn_t *conn = nullptr;
    int tries = 0;
    
    if (is_client_) {
        // Act as client - connect to remote endpoint
        std::cout << "Acting as CLIENT, connecting to " << ip_addr << ":" << port << "?gpu=" << gpu_index << std::endl;
        do {
            conn = uccl_engine_connect(engine_, ip_addr, gpu_index, port);
            tries++;
            if (!conn && tries < MAX_RETRIES) {

            }
        } while(!conn && tries < MAX_RETRIES);

        if (!conn) {
            std::cerr << "Failed to connect to remote agent " << remote_agent << " after " << MAX_RETRIES << " attempts" << std::endl;
            delete[] ip_addr;
            return NIXL_ERR_BACKEND;
        }
    } else {
        // Act as server - accept incoming connection
        std::cout << "Acting as SERVER, accepting connection from " << ip_addr << ":" << port << "?gpu=" << gpu_index << std::endl;
        char ip_buf[256];
        int remote_gpu_idx;
        conn = uccl_engine_accept(engine_, ip_buf, sizeof(ip_buf), &remote_gpu_idx);
        if (!conn) {
            std::cerr << "Failed to accept connection from remote agent " << remote_agent << std::endl;
            delete[] ip_addr;
            return NIXL_ERR_BACKEND;
        }
    }
    std::cout << "Successfully connected to remote agent " << remote_agent << std::endl;
    // Start the listener thread for receiving metadata during postXfer
    uccl_engine_start_listener(conn);

    connected_agents_[remote_agent] = reinterpret_cast<uint64_t>(conn);

    delete[] ip_addr;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::connect(const std::string &remote_agent) {
    // Unused 
    std::cout << "Connecting to remote_agent: "<<remote_agent<< std::endl;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::disconnect(const std::string &remote_agent) {
    // TODO: Disconnect from remote agent if needed
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::registerMem(const nixlBlobDesc &mem, const nixl_mem_t &nixl_mem, nixlBackendMD* &out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mem_reg_info_.count(mem.addr)) {
        auto priv = mem_reg_info_[mem.addr];
        std::cout << "Registering memory: "<<mem.addr<<" ref_cnt: "<<priv->ref_cnt<< std::endl;
        priv->ref_cnt++;
        out = priv;
        return NIXL_SUCCESS;
    }
    
    // Register memory with UCCL engine
    uccl_mr_t* mr = uccl_engine_reg(engine_, mem.addr, mem.len);
    if (!mr) {
        std::cerr << "Failed to register memory with UCCL engine" << std::endl;
        return NIXL_ERR_BACKEND;
    }
    
    auto priv = new nixlUcclBackendMD(true);
    priv->addr = (void *) mem.addr;
    priv->length = mem.len;
    priv->ref_cnt = 1;
    priv->mr_id = reinterpret_cast<uint64_t>(mr); // Store the memory region handle
    out = priv;
    mem_reg_info_[mem.addr] = priv;
    std::cout << "Registering memory: "<<mem.addr<<" ref_cnt: "<<priv->ref_cnt<<" mr_id: "<<priv->mr_id<< std::endl;

    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::deregisterMem(nixlBackendMD* meta) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto priv = static_cast<nixlUcclBackendMD*>(meta);
    priv->ref_cnt--;
    if (priv->ref_cnt > 0) return NIXL_SUCCESS;
    
    // Deregister memory from UCCL engine
    if (priv->mr_id != 0) {
        uccl_mr_t* mr = reinterpret_cast<uccl_mr_t*>(priv->mr_id);
        uccl_engine_mr_destroy(mr);
        std::cout << "Deregistered memory: "<<priv->addr<<" mr_id: "<<priv->mr_id<< std::endl;
    }
    
    mem_reg_info_.erase((uint64_t)priv->addr);
    delete priv;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::loadLocalMD(nixlBackendMD* input, nixlBackendMD* &output) {
    // No-op for UCCL
    output = nullptr;
    std::cout << "LoadLocalMD: "<< std::endl;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::loadRemoteMD(const nixlBlobDesc &input, const nixl_mem_t &nixl_mem, const std::string &remote_agent, nixlBackendMD* &output) {
    output = nullptr;
    std::cout << "LoadRemoteMD: remote_agent: "<<remote_agent<< std::endl;
    std::cout << "nixlBlobDesc input - addr: " << input.addr << ", len: " << input.len << std::endl;
    
    // Client would be invoking postXfer to perform write/read.
    
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::unloadMD(nixlBackendMD* input) {
    // No-op for UCCL
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::prepXfer(const nixl_xfer_op_t &operation, const nixl_meta_dlist_t &local, const nixl_meta_dlist_t &remote, const std::string &remote_agent, nixlBackendReqH* &handle, const nixl_opt_b_args_t* opt_args) const {
    // Prepare a transfer handle (not used in this stub)
    handle = nullptr;
    std::cout << "PrepXfer: "<<operation<<" remote_agent: "<<remote_agent<< std::endl;
    // Get the connection for this remote agent
    auto conn_iter = connected_agents_.find(remote_agent);
    if (conn_iter == connected_agents_.end()) {
        std::cerr << "No connection found for remote agent: " << remote_agent << std::endl;
        return NIXL_ERR_BACKEND;
    }
    uccl_conn_t* conn = reinterpret_cast<uccl_conn_t*>(conn_iter->second);
    if (!conn) {
        std::cerr << "Invalid connection for remote agent: " << remote_agent << std::endl;
        return NIXL_ERR_BACKEND;
    }
    
    size_t lcnt = local.descCount();
    size_t rcnt = remote.descCount();
    
    if (lcnt != rcnt) {
        std::cerr << "Local and remote descriptor counts don't match: " << lcnt << " != " << rcnt << std::endl;
        return NIXL_ERR_INVALID_PARAM;
    }
    
    for (size_t i = 0; i < lcnt; i++) {
        void* laddr = (void*)local[i].addr;
        size_t lsize = local[i].len;
        void* raddr = (void*)remote[i].addr;
        size_t rsize = remote[i].len;
        
        std::cout << "Local address: " << laddr << " size: " << lsize << " Remote address: " << raddr << " size: " << rsize << std::endl;
        // Send the memory region metadata to the remote agent
        // TODO: Send other params too
        metadata_t md = metadata_t{
            .data_ptr = (uint64_t)raddr,
            .data_size = rsize
        };
        int sock_fd = uccl_engine_get_sock_fd(conn);
        // Send the message to receiver of where to receive the upcoming data
        if (sock_fd >= 0) {
            send(sock_fd, &md, sizeof(metadata_t), 0);
        }
    }
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::postXfer(const nixl_xfer_op_t &operation, const nixl_meta_dlist_t &local, const nixl_meta_dlist_t &remote, const std::string &remote_agent, nixlBackendReqH* &handle, const nixl_opt_b_args_t* opt_args) const {
    std::cout << "PostXfer: "<<operation<<" remote_agent: "<<remote_agent<< std::endl;
    
    // Get the connection for this remote agent
    auto conn_iter = connected_agents_.find(remote_agent);
    if (conn_iter == connected_agents_.end()) {
        std::cerr << "No connection found for remote agent: " << remote_agent << std::endl;
        return NIXL_ERR_BACKEND;
    }
    
    uccl_conn_t* conn = reinterpret_cast<uccl_conn_t*>(conn_iter->second);
    if (!conn) {
        std::cerr << "Invalid connection for remote agent: " << remote_agent << std::endl;
        return NIXL_ERR_BACKEND;
    }
    
    size_t lcnt = local.descCount();
    size_t rcnt = remote.descCount();
    
    if (lcnt != rcnt) {
        std::cerr << "Local and remote descriptor counts don't match: " << lcnt << " != " << rcnt << std::endl;
        return NIXL_ERR_INVALID_PARAM;
    }
    
    // Process each descriptor pair
    for (size_t i = 0; i < lcnt; i++) {
        void* laddr = (void*)local[i].addr;
        size_t lsize = local[i].len;
        void* raddr = (void*)remote[i].addr;
        size_t rsize = remote[i].len;
        
        std::cout << "Local address: " << laddr << " size: " << lsize << " Remote address: " << raddr << " size: " << rsize << std::endl;
        //send the memory region metadata to the remote agent

        if (lsize != rsize) {
            std::cerr << "Local and remote sizes don't match: " << lsize << " != " << rsize << std::endl;
            return NIXL_ERR_INVALID_PARAM;
        }
        
        // Get local memory region
        auto local_mem_iter = mem_reg_info_.find(local[i].addr);
        if (local_mem_iter == mem_reg_info_.end()) {
            std::cerr << "Local memory not registered for address: " << local[i].addr << std::endl;
            return NIXL_ERR_BACKEND;
        }
        
        auto local_priv = local_mem_iter->second;
        if (local_priv->mr_id == 0) {
            std::cerr << "Local memory region not properly registered" << std::endl;
            return NIXL_ERR_BACKEND;
        }
        
        uccl_mr_t* local_mr = reinterpret_cast<uccl_mr_t*>(local_priv->mr_id);
        
        int result = 0;
        
        switch (operation) {
        case NIXL_READ:
            std::cout << "Performing READ operation: receiving " << lsize << " bytes" << std::endl;
            result = uccl_engine_recv(conn, local_mr, laddr, lsize);
            break;
            
        case NIXL_WRITE:
            std::cout << "Performing WRITE operation: sending " << lsize << " bytes" << std::endl;
            result = uccl_engine_send(conn, local_mr, laddr, lsize);
            break;
            
        default:
            std::cerr << "Unsupported operation type: " << operation << std::endl;
            return NIXL_ERR_INVALID_PARAM;
        }
        
        if (result != 0) {
            std::cerr << "UCCL operation failed with result: " << result << std::endl;
            return NIXL_ERR_BACKEND;
        }
        
        std::cout << "Successfully completed " << (operation == NIXL_READ ? "READ" : "WRITE") 
                  << " operation: " << lsize << " bytes" << std::endl;
    }
    
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::checkXfer(nixlBackendReqH* handle) const {
    // TODO: Check transfer status if async
    std::cout << "CheckXfer: "<< std::endl;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::releaseReqH(nixlBackendReqH* handle) const {
    // TODO: Release any resources associated with the transfer handle  
    std::cout << "ReleaseReqH: "<< std::endl;
    return NIXL_SUCCESS;
} 