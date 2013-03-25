#include "../../../src/mirall/utility.h"

#include <QDir>

int main(int argc, char* argv[])
{
   QString dir="/tmp/linktest/";
   QDir().mkpath(dir);
   Mirall::Utility::setupFavLink(dir);
}
