#pragma once
#include <Arduino.h>

String getStatusJSON();
void setupWebServer();
void webServerLoop();   // call from main loop() — runs ElegantOTA
