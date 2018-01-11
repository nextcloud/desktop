#!groovy

//
// We now run the tests in Debug mode so that ASSERTs are triggered.
// Ideally we should run the tests in both Debug and Release so we catch
// all possible error combinations.
// See also the top comment in syncenginetestutils.h
//
// We are building "Linux - GCC" with "make" and "Linux - Clang" with ninja,
// the combinations are more or less arbitrarily chosen. We just want to
// check that both compilers and both CMake generators work. It's
// unlikely that a specific generator only breaks with a specific
// compiler.


def linux = docker.image('dominikschmidt/owncloud-client-ci-image:latest')
def win32 = docker.image('guruz/docker-owncloud-client-win32:latest')

node('CLIENT') {
    stage 'Checkout'
        checkout scm
        sh '''git submodule update --init'''

    stage 'Linux - GCC'
        linux.pull()
        linux.inside {
            sh '''
            export HOME="$(pwd)/home"
            rm -rf build home
            mkdir build
            cd build
            cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE="Debug" -DUNIT_TESTING=1 ..
            make -j4
            ctest -V --output-on-failure
            '''
        }

    stage 'Linux - Clang'
        linux.pull()
        linux.inside {
            sh '''
            export HOME="$(pwd)/home"
            rm -rf build home
            mkdir build
            cd build
            cmake -GNinja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE="Debug" -DUNIT_TESTING=1 ..
            ninja -j4
            ctest -V --output-on-failure
            '''
        }

    stage 'Win32'
        win32.pull() // make sure we have the latest available from Docker Hub
        win32.inside {
            sh '''
            rm -rf build-win32
            mkdir build-win32
            cd build-win32
            ../admin/win/download_runtimes.sh
            cmake .. -DCMAKE_TOOLCHAIN_FILE=../admin/win/Toolchain-mingw32-openSUSE.cmake -DWITH_CRASHREPORTER=ON
            make -j4
            make package
            ctest .
            '''
        }

   // Stage 'macOS' TODO
}
