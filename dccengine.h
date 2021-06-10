#ifndef __DCCENGINE_H__
#define __DCCENGINE_H__


std::string dccInit();
std::string dccCommand(std::string cmd); //goes in some sort of loop to feed it commands...
void dccFinish();

#endif