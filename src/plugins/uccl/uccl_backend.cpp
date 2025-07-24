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

nixlUcclEngine::nixlUcclEngine(const nixlBackendInitParams* init_params)
    : nixlBackendEngine(init_params) {
    local_agent_name_ = init_params->localAgent;
    // TODO: Initialize UCCL engine with appropriate GPU index and CPU count
    // For now, use dummy values (0, 1). But extend it to all devices
    engine_ = uccl_engine_create(0, 1);
    std::cout << "UCCL engine created" << std::endl;
}

nixlUcclEngine::~nixlUcclEngine() {
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
        return NIXL_ERROR;
    }
    
    char* metadata = nullptr;
    int result = uccl_engine_get_metadata(engine_, &metadata);
    if (result != 0 || !metadata) {
        return NIXL_ERROR;
    }
    
    str = std::string(metadata);
    delete[] metadata;
    std::cout << "UCCL engine metadata: " << str << std::endl;
    return NIXL_SUCCESS;
}


nixl_status_t nixlUcclEngine::loadRemoteConnInfo(const std::string &remote_agent, const std::string &remote_conn_info) {
    // TODO: Parse remote_conn_info and establish connection using Endpoint
    // For now, just store a dummy conn_id
    std::cout << "UCCL engine remote_agent: "<<remote_agent<<" loadRemoteConnInfo: " << remote_conn_info << std::endl;
    std::lock_guard<std::mutex> lock(mutex_);
    connected_agents_[remote_agent] = 0; // Placeholder conn_id
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::connect(const std::string &remote_agent) {
    // TODO: Actually connect to remote agent using Endpoint
    // For now, assume connection is always successful
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
        priv->ref_cnt++;
        out = priv;
        return NIXL_SUCCESS;
    }
    std::cout << "Registering memory: "<<mem.addr<<" ref_cnt: "<<priv->ref_cnt<< std::endl;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::deregisterMem(nixlBackendMD* meta) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto priv = static_cast<nixlUcclBackendMD*>(meta);
    priv->ref_cnt--;
    if (priv->ref_cnt > 0) return NIXL_SUCCESS;
    // TODO: Deregister memory from UCCL Endpoint if needed
    mem_reg_info_.erase((uint64_t)priv->addr);
    delete priv;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::loadLocalMD(nixlBackendMD* input, nixlBackendMD* &output) {
    // No-op for UCCL
    output = nullptr;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::loadRemoteMD(const nixlBlobDesc &input, const nixl_mem_t &nixl_mem, const std::string &remote_agent, nixlBackendMD* &output) {
    // No-op for UCCL
    output = nullptr;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::unloadMD(nixlBackendMD* input) {
    // No-op for UCCL
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::prepXfer(const nixl_xfer_op_t &operation, const nixl_meta_dlist_t &local, const nixl_meta_dlist_t &remote, const std::string &remote_agent, nixlBackendReqH* &handle, const nixl_opt_b_args_t* opt_args) const {
    // Prepare a transfer handle (not used in this stub)
    handle = nullptr;
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::postXfer(const nixl_xfer_op_t &operation, const nixl_meta_dlist_t &local, const nixl_meta_dlist_t &remote, const std::string &remote_agent, nixlBackendReqH* &handle, const nixl_opt_b_args_t* opt_args) const {
    // TODO: Use Endpoint to perform send/recv based on operation
    // For now, just return success
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::checkXfer(nixlBackendReqH* handle) const {
    // TODO: Check transfer status if async
    return NIXL_SUCCESS;
}

nixl_status_t nixlUcclEngine::releaseReqH(nixlBackendReqH* handle) const {
    // TODO: Release any resources associated with the transfer handle
    return NIXL_SUCCESS;
} 