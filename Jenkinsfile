#!groovy

node('CLIENT') {
    stage 'Checkout'
        checkout scm
        sh '''git submodule update --init'''

    stage 'Qt4'
        sh '''rm -rf build
		mkdir build
		cd build
		cmake -DUNIT_TESTING=1 -DBUILD_WITH_QT4=ON ..
		make
		ctest --output-on-failure'''

    stage 'Qt4 - clang'
        sh '''rm -rf build
		mkdir build
		cd build
		cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DUNIT_TESTING=1 -DBUILD_WITH_QT4=ON ..
		make
		ctest --output-on-failure'''

    stage 'Qt5'
        sh '''rm -rf build
		mkdir build
		cd build
		cmake -DUNIT_TESTING=1 -DBUILD_WITH_QT4=OFF ..
		make
		ctest --output-on-failure'''

    stage 'Qt5 - clang'
        sh '''rm -rf build
		mkdir build
		cd build
		cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DUNIT_TESTING=1 -DBUILD_WITH_QT4=OFF ..
		make
		ctest --output-on-failure'''


}


