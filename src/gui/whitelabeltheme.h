#ifndef WHITELABELTHEME_H
#define WHITELABELTHEME_H
// // #define WHITELABEL_IONOS
// #define WHITELABEL_STRATO

#include "basetheme.h"
#include "stratotheme.h"
#include "ionostheme.h"

namespace OCC {


#if defined(IONOS_WL_BUILD)
    static IonosTheme WLTheme;    
#else
    static StratoTheme WLTheme;
#endif

} // namespace OCC

#endif // WHITELABELTHEME_H
