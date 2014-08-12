// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <iostream>
#include <vector>
#include <string>

#include "RemotePathChecker.h"
#include "StringUtil.h"

using namespace std;

int _tmain(int argc, _TCHAR* argv[])
{
	RemotePathChecker checker(33001);

	vector<wstring> paths;

	wstring test1(L"C:\\Users\\owncloud\\ownCloud\\wizard2.png");
	wstring test2(L"C:\\Users\\owncloud\\ownCloud\\wizard3.png");
	wstring test3(L"C:\\Users\\owncloud\\ownCloud\\HAMMANET.png");
	paths.push_back(test1);
	paths.push_back(test2);
	paths.push_back(test3);

//	wstring test3 = StringUtil::toUtf16(StringUtil::toUtf8(test1.c_str()));

	vector<wstring>::iterator it;
	for (it = paths.begin(); it != paths.end(); ++it) {
		bool monitored = checker.IsMonitoredPath(it->c_str(), false);
		wcout << *it << " " << monitored << " with value " << checker.GetPathType() << endl;
	}
	return 0;
}

