//
// Created by 阳坤 on 2020-08-19.
//

#include "AVToolsBuilder.h"

IPlayerProxy *AVToolsBuilder::getPlayEngine(unsigned char index) {
    return IPlayerProxy::getInstance();
}

SoundTouchUtils *AVToolsBuilder::getSoundTouchEngine() {
    return SoundTouchUtils::getInstance();
}
