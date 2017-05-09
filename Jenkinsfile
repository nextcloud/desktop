#!groovy

node('CLIENT') {
    stage 'Checkout'
        checkout scm
        sh '''git submodule update --init'''

    stage 'Qt5'
        sh '''rm -rf build
		mkdir build
		cd build
		cmake -DUNIT_TESTING=1 -DBUILD_WITH_QT4=OFF ..
		make -j4
		ctest --output-on-failure'''

    stage 'Qt5 - clang'
        sh '''rm -rf build
		mkdir build
		cd build
		cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DUNIT_TESTING=1 -DBUILD_WITH_QT4=OFF ..
		make -j4
		ctest --output-on-failure'''


    stage 'Win32'
	def win32 = docker.image('guruz/docker-owncloud-client-win32:latest')
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
}


