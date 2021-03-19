
def rocmnode(name) {
    def node_name = 'rocmtest'
    if(name == 'vega') {
        node_name = 'rocmtest-trial && vega';
    } else if(name == 'vega10') {
        node_name = 'rocmtest-trial && vega10';
    } else if(name == 'vega20') {
        node_name = 'rocmtest-trial && vega20';
    } else if(name == 'gfx908') {
        node_name = 'rocmtest-trial && gfx908';
    } else {
        node_name = name + '-trial'
    }
    return node_name
}

def default_image_name() {
    return 'miopen-hip-clang'
}

def show_node_info() {
    sh """
        echo "NODE_NAME = \$NODE_NAME"
        lsb_release -sd
        uname -r
        cat /sys/module/amdgpu/version
        ls /opt/ -la
    """
}

def cmake_build(compiler, flags, env4make, extradebugflags, prefixpath){
    def workspace_dir = pwd()
    def vcache = "/var/jenkins/.cache/miopen/vcache"
    def archive = (flags == '-DCMAKE_BUILD_TYPE=release')
    def config_targets = "check doc MIOpenDriver"
    def test_flags = "--disable-verification-cache"
    def debug_flags = "-g ${extradebugflags} -fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize-recover=undefined"
    def compilerpath = ""
    def configargs = ""

    compilerpath = compiler;
    if (prefixpath != "/usr/local") {
        configargs = "-DCMAKE_PREFIX_PATH=${prefixpath}"
    }

    if (archive == true) {
        config_targets = "package"
    }
    def cmd = """
        echo \$HSA_ENABLE_SDMA
        ulimit -c unlimited
        rm -rf build
        mkdir build
        cd build
        CXX=${compilerpath} CXXFLAGS='-Werror' cmake ${configargs} -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_FLAGS='${test_flags}' -DCMAKE_CXX_FLAGS_DEBUG='${debug_flags}' ${flags} ..
        MIOPEN_DEBUG_CONV_IMPLICIT_GEMM_XDLOPS=1 CTEST_PARALLEL_LEVEL=4 MIOPEN_VERIFY_CACHE_PATH=${vcache} MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 ${env4make} dumb-init make -j\$(nproc) ${config_targets}
    """
    echo cmd
    sh cmd
    // Only archive from master or develop
    if (archive == true && (env.BRANCH_NAME == "develop" || env.BRANCH_NAME == "master")) {
        archiveArtifacts artifacts: "build/*.deb", allowEmptyArchive: true, fingerprint: true
    }
}

def buildHipClangJob(Map conf, compiler){
        show_node_info()

        env.HSA_ENABLE_SDMA=0
        env.CODECOV_TOKEN="aec031be-7673-43b5-9840-d8fb71a2354e"
        checkout scm
        def prefixpath = conf.get("prefixpath", "/usr/local")
        def flags = conf.get("flags", "")
        def env4make = conf.get("env4make", "")
        def image = conf.get("image", default_image_name())
        def cmd = conf.get("cmd", "")
        def gpu_arch = conf.get("gpu_arch", "gfx900;gfx906")
        def codecov = conf.get("codecov", false)
        def miotensile_version = conf.get("miotensile_version", "default")
        def dockerOpts="--device=/dev/kfd --device=/dev/dri --group-add video --cap-add=SYS_PTRACE --security-opt seccomp=unconfined"
        def dockerArgs = "--build-arg PREFIX=${prefixpath} --build-arg GPU_ARCH='${gpu_arch}' --build-arg MIOTENSILE_VER='${miotensile_version}' "
        def extradebugflags = ""
        def variant = env.STAGE_NAME
        if (codecov) {
            extradebugflags = "-fprofile-arcs -ftest-coverage"
        }
        def retimage
        gitStatusWrapper(credentialsId: '7126e5fe-eb51-4576-b52b-9aaf1de8f0fd', gitHubContext: "Jenkins - ${variant}", account: 'ROCmSoftwarePlatform', repo: 'MIOpen') {
            try {
                retimage = docker.build("${image}", dockerArgs + '.')
                withDockerContainer(image: image, args: dockerOpts) {
                    timeout(time: 5, unit: 'MINUTES')
                    {
                        sh 'PATH="/opt/rocm/opencl/bin:/opt/rocm/opencl/bin/x86_64:$PATH" clinfo'
                    }
                }
            }
            catch (org.jenkinsci.plugins.workflow.steps.FlowInterruptedException e){
                echo "The job was cancelled or aborted"
                throw e
            }
            catch(Exception ex) {
                retimage = docker.build("${image}", dockerArgs + "--no-cache .")
                withDockerContainer(image: image, args: dockerOpts) {
                    timeout(time: 5, unit: 'MINUTES')
                    {
                        sh 'PATH="/opt/rocm/opencl/bin:/opt/rocm/opencl/bin/x86_64:$PATH" clinfo'
                    }
                }
            }

            withDockerContainer(image: image, args: dockerOpts + ' -v=/var/jenkins/:/var/jenkins') {
                timeout(time: 5, unit: 'HOURS')
                {
                    if(cmd == ""){
                        cmake_build(compiler, flags, env4make, extradebugflags, prefixpath)
                    }else{
                        sh cmd
                    }
                    if (codecov) {
                        sh '''
                            cd build
                            lcov --directory . --capture --output-file $(pwd)/coverage.info
                            lcov --remove $(pwd)/coverage.info '/usr/*' --output-file $(pwd)/coverage.info
                            lcov --list $(pwd)/coverage.info
                            curl -s https://codecov.io/bash | bash
                            echo "Uploaded"
                        '''
                    }
                }
            }
        }
        return retimage
}

def reboot(){
    build job: 'reboot-slaves', propagate: false , parameters: [string(name: 'server', value: "${env.NODE_NAME}"),]
}

/// Stage name format:
/// [DataType] Backend[/Compiler] BuildType [TestSet] [Target]
///
/// The only mandatory elements are Backend and BuildType; others are optional.
///
/// DataType := { Half | BF16 | Int8 | FP32* }
///   * "FP32" is the default and usually not specified.
/// Backend := { Hip | OpenCL | HipNoGPU}
/// Compiler := { Clang* | GCC* }
///   * "Clang" is the default for the Hip backend, and implies hip-clang compiler.
///     For the OpenCL backend, "Clang" implies the system x86 compiler.
///   * "GCC" is the default for OpenCL backend.
///   * The default compiler is usually not specified.
/// BuildType := { Release | Debug [ BuildTypeModifier ] }
///   * BuildTypeModifier := { COMGR | Embedded | Static | Normal-Find | Fast-Find
///                                  | Tensile | Tensile-Latest | Package | ... }
/// TestSet := { All | Subset | Smoke* } [ Codecov ]
///   * "All" corresponds to "cmake -DMIOPEN_TEST_ALL=On".
///   * "Subset" corresponds to Target- or BuildTypeModifier-specific subsetting of
///     the "All" testset, e.g. -DMIOPEN_TEST_GFX908=On or -DMIOPEN_TEST_MIOTENSILE=On.
///   * "Smoke" (-DMIOPEN_TEST_ALL=Off) is the default and usually not specified.
///   * "Codecov" is optional code coverage analysis.
/// Target := { gfx908 | Vega20 | Vega10 | Vega* }
///   * "Vega" (gfx906 or gfx900) is the default and usually not specified.

pipeline {
    agent none
    options {
        parallelsAlwaysFailFast()
    }
    environment{
        image = default_image_name()
    }
    stages{
        // Run all static analysis tests
        stage("Static checks"){
            parallel{
                stage('Clang Hip Tidy') {
                    agent{  label rocmnode("rocmtest") }
                    environment{
                        cmd = "rm -rf build; mkdir build; cd build; CXX='/opt/rocm/llvm/bin/clang++' cmake -DMIOPEN_BACKEND=HIP -DBUILD_DEV=On ..; make -j\$(nproc) -k analyze;"
                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('clang++', flags: '-DCMAKE_BUILD_TYPE=release', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                        }
                    }
                }
            }
        }
        // Run quick fp32 tests
        stage("Fast full precision"){
            parallel{
               stage('OpenCL Debug') {
                    agent{ label rocmnode("vega") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('g++', flags: '-DBUILD_DEV=On -DCMAKE_BUILD_TYPE=debug')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('OpenCL Release') {
                    agent{ label rocmnode("vega") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('g++', flags: '-DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('Hip Release /opt/rocm') {
                    agent{ label rocmnode("vega") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', flags: '-DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release', image: image+'rocm', prefixpath: '/opt/rocm')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('Hip Debug') {
                    agent{ label rocmnode("vega") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=debug -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_FLAGS=--disable-verification-cache ..
                            CTEST_PARALLEL_LEVEL=4 MIOPEN_DEBUG_IMPLICIT_GEMM_NON_XDLOPS_INLINE_ASM=0 MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 make -j\$(nproc) check
                        """

                    }
                    steps{
                        script{
                            try{
                                 buildHipClangJob('/opt/rocm/llvm/bin/clang++', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }



            }
        }

        // Misc tests
        stage("Aux tests"){
            parallel{
                stage('Hip Debug COMGR') {
                    agent{ label rocmnode("vega") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DMIOPEN_USE_COMGR=On -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=debug -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_FLAGS='--verbose --disable-verification-cache' ..
                            CTEST_PARALLEL_LEVEL=2 MIOPEN_DEBUG_IMPLICIT_GEMM_NON_XDLOPS_INLINE_ASM=0 MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 MIOPEN_LOG_LEVEL=5 make -j\$(nproc) check
                        """

                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }
                stage('Hip Debug Embedded Vega20') {
                    agent{ label rocmnode("vega20") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DMIOPEN_EMBED_DB="gfx906_60;gfx906_64" -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=debug -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_FLAGS='--verbose --disable-verification-cache' ..
                            MIOPEN_LOG_LEVEL=5 make -j\$(nproc) check
                        """

                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }


                    }
                }

                stage('Hip Release Static') {
                    agent{ label rocmnode("vega") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DBUILD_SHARED_LIBS=Off -DCMAKE_BUILD_TYPE=release -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_FLAGS=--disable-verification-cache ..
                            CTEST_PARALLEL_LEVEL=4 MIOPEN_DEBUG_IMPLICIT_GEMM_NON_XDLOPS_INLINE_ASM=0 MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 make -j\$(nproc) check
                        """

                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('Hip Release Normal-Find') {
                    agent{ label rocmnode("vega") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release -DMIOPEN_GPU_SYNC=On ..
                            make -j test_conv2d
                            MIOPEN_FIND_MODE=1 CTEST_PARALLEL_LEVEL=4 MIOPEN_DEBUG_IMPLICIT_GEMM_NON_XDLOPS_INLINE_ASM=0 MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 bin/test_conv2d --disable-verification-cache
                        """
                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('Hip Release Fast-Find') {
                    agent{ label rocmnode("vega") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release -DMIOPEN_GPU_SYNC=On ..
                            make -j test_conv2d
                            MIOPEN_FIND_MODE=2 CTEST_PARALLEL_LEVEL=4 MIOPEN_DEBUG_IMPLICIT_GEMM_NON_XDLOPS_INLINE_ASM=0 MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 bin/test_conv2d --disable-verification-cache
                        """
                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('Hip Release') {
                    agent{ label rocmnode("vega") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', flags: '-DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

            }
        }

        // Run fp16, bfp16, and int8 quick tests
        stage("Fast low precision"){
            parallel{
                stage('Half Hip Release Vega20 /opt/rocm') {
                    agent{ label rocmnode("vega20") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', flags: '-DMIOPEN_TEST_HALF=On -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release', image: image+'rocm', prefixpath: '/opt/rocm')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }
                stage('Half OpenCL Release Vega20') {
                    agent{ label rocmnode("vega20") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('g++', flags: '-DMIOPEN_TEST_HALF=On -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }
                stage('Int8 OpenCL Release Vega20') {
                    agent{ label rocmnode("vega20") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('g++', flags: '-DMIOPEN_TEST_INT8=On -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('BF16 Hip Release Vega20 /opt/rocm') {
                    agent{ label rocmnode("vega20") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', flags: '-DMIOPEN_TEST_BFLOAT16=On -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release', image: image+'rocm', prefixpath: '/opt/rocm')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

            }
        }

        stage("Full tests I"){
            parallel{

                stage('OpenCL Debug + Codecov') {
                    agent{ label rocmnode("vega") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('g++', flags: '-DBUILD_DEV=On -DCMAKE_BUILD_TYPE=debug', codecov: true)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('Int8 Hip Release All Vega20 /opt/rocm') {
                    agent{ label rocmnode("vega20") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', flags: '-DMIOPEN_TEST_INT8=On -DBUILD_DEV=On -DMIOPEN_TEST_ALL=On -DCMAKE_BUILD_TYPE=release', image: image+'rocm', prefixpath: '/opt/rocm')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

            }
        }

        stage("Full tests II"){
            parallel{
                stage('OpenCL Release All') {
                    agent{ label rocmnode("vega") }
                    steps{
                        script{
                            try{
                                buildHipClangJob('g++', flags: '-DBUILD_DEV=On -DMIOPEN_TEST_ALL=On -DCMAKE_BUILD_TYPE=release')
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

            }
        }

        stage("Full tests III"){
            parallel{
                stage('Half Hip Release All Vega20') {
                    agent{ label rocmnode("vega20") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release -DMIOPEN_TEST_HALF=On -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_ALL=On -DMIOPEN_TEST_FLAGS=--disable-verification-cache ..
                            CTEST_PARALLEL_LEVEL=4 MIOPEN_DEBUG_IMPLICIT_GEMM_NON_XDLOPS_INLINE_ASM=0 MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 make -j\$(nproc) check
                        """

                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('Hip Release All Vega20') {
                    agent{ label rocmnode("vega20") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_ALL=On -DMIOPEN_TEST_FLAGS=--disable-verification-cache ..
                            CTEST_PARALLEL_LEVEL=4 MIOPEN_DEBUG_IMPLICIT_GEMM_NON_XDLOPS_INLINE_ASM=0 MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 make -j\$(nproc) check
                        """

                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }
            }
        }

        stage("MIOpenTensile"){
            parallel{
                stage('Hip Release Tensile Subset Vega20') {
                    agent{ label rocmnode("vega20") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_ALL=On -DMIOPEN_TEST_MIOTENSILE=ON -DMIOPEN_USE_MIOPENTENSILE=ON -DMIOPEN_USE_ROCBLAS=OFF -DMIOPEN_TEST_FLAGS=--disable-verification-cache ..
                            MIOPEN_DEBUG_HIP_KERNELS=0 CTEST_PARALLEL_LEVEL=4 MIOPEN_DEBUG_IMPLICIT_GEMM_NON_XDLOPS_INLINE_ASM=0 MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 make -j\$(nproc) check
                        """
                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', cmd: cmd)
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

                stage('Hip Release Tensile-Latest Subset Vega20') {
                    agent{ label rocmnode("vega20") }
                    environment{
                        cmd = """
                            ulimit -c unlimited
                            rm -rf build
                            mkdir build
                            cd build
                            CXX=/opt/rocm/llvm/bin/clang++ cmake -DBUILD_DEV=On -DCMAKE_BUILD_TYPE=release -DMIOPEN_GPU_SYNC=On -DMIOPEN_TEST_ALL=On -DMIOPEN_TEST_MIOTENSILE=ON -DMIOPEN_USE_MIOPENTENSILE=ON -DMIOPEN_USE_ROCBLAS=OFF -DMIOPEN_TEST_FLAGS=--disable-verification-cache ..
                            MIOPEN_DEBUG_HIP_KERNELS=0 CTEST_PARALLEL_LEVEL=4 MIOPEN_DEBUG_IMPLICIT_GEMM_NON_XDLOPS_INLINE_ASM=0 MIOPEN_CONV_PRECISE_ROCBLAS_TIMING=0 make -j\$(nproc) check
                        """
                    }
                    steps{
                        script{
                            try{
                                buildHipClangJob('/opt/rocm/llvm/bin/clang++', image: image+'-gfx906-miotensilelatest', cmd: cmd, gpu_arch: "gfx906", miotensile_version: "latest")
                            }
                            catch(e){
                                echo "throwing error exception for the stage"
                                echo 'Exception occurred: ' + e.toString()
                                throw e
                            }
                            finally{
                                reboot()
                            }
                        }
                    }
                }

            }
        }

    }
}

