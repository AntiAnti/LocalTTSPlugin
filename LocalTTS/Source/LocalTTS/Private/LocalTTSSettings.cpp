// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LocalTTSSettings.h"

const UTtsSettings* UTtsSettings::Get()
{
    return GetDefault<UTtsSettings>();
}
