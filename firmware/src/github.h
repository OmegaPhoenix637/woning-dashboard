#pragma once
#include <Arduino.h>

// Upload naar de privé data-repo via de GitHub Git Data API.
//  - live-branch: live.json + today.json, elke keer een nieuwe commit zonder geschiedenis (force)
//  - main-branch: afgesloten dagen (days/JJJJ/datum.json) + jaarsamenvatting (summary/JJJJ.json)
bool ghPublishLive(String& err);
bool ghPublishPending(String& err);
uint32_t ghLastLiveOk();
