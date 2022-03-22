#pragma once

#include "abstractcorejob.h"

namespace OCC::Wizard::Jobs {

class ResolveUrlJobFactory : public AbstractCoreJobFactory
{
public:
    explicit ResolveUrlJobFactory(QNetworkAccessManager *nam, QObject *parent = nullptr);

    CoreJob *startJob(const QUrl &url) override;
};

}
