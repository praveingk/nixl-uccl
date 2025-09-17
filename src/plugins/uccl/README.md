## UCCL Backend Plugin [Preview]

[UCCL](https://github.com/uccl-project/uccl) is an efficient communication library to perform GPU memory transfers, with a focus on flexibility (evolving ML workloads) and portability  (heteregenous GPUs).

## Usage Guide
1. Build the install UCCL manually. You can refer to the [installation guide here](https://https://github.com/uccl-project/uccl).

    ```cpp
    git clone https://github.com/uccl-project/uccl.git
    cd uccl/p2p
    make -j
    sudo make install
    ```

2. Build NIXL.

3. To test the Mooncake backend, you can run the unit test in `test/unit/plugins/mooncake/mooncake_backend_test`.

4. To use the Notify feature, you need to download the latest main branch of Mooncake.