//
// Created by dkulpa on 09.12.23.
//

#ifndef MGLIGHTFW_SYSTEMUPGRADER_H
#define MGLIGHTFW_SYSTEMUPGRADER_H

#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include "consts.h"

class SystemUpgrader {
public:
    enum Result {Ok, Failed, HwMismatch};
    static Result upgrade(WiFiClientSecure& cli, int v, const String &id, const String &picklock, int uid);
};


#endif //MGLIGHTFW_SYSTEMUPGRADER_H
