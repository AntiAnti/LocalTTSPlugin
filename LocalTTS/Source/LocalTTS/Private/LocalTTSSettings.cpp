// (c) Yuri N. K. 2025. All rights reserved.
// ykasczc@gmail.com

#include "LocalTTSSettings.h"
#include "Phonemizer.h"
#include "NNEModelData.h"

UTtsSettings::UTtsSettings()
    : PhonemizerEncoder(FSoftObjectPath(TEXT("/LocalTTS/G2P/g2p_encoder_tiny.g2p_encoder_tiny")))
    , PhonemizerDecoder(FSoftObjectPath(TEXT("/LocalTTS/G2P/g2p_decoder_tiny.g2p_decoder_tiny")))
    , PhonemizerInfo(FSoftObjectPath(TEXT("/LocalTTS/G2P/g2p_info.g2p_info")))
{}

const UTtsSettings* UTtsSettings::Get()
{
    return GetDefault<UTtsSettings>();
}
