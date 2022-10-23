#include "pch.h"
#include <EventAPI.h>
#include <LoggerAPI.h>
#include <MC/Level.hpp>
#include <MC/BlockInstance.hpp>
#include <MC/Block.hpp>
#include <MC/BlockSource.hpp>
#include <MC/Actor.hpp>
#include <MC/Player.hpp>
#include <MC/ItemStack.hpp>
#include "Version.h"
#include <LLAPI.h>
#include <ServerAPI.h>
#include <MC/FishingHook.hpp>
//#include <MC/FishingRodItem.hpp>

Logger logger(PLUGIN_NAME);

int fishinghook_offset = 0;
int tickcount = 0;
std::unordered_map<Player*, BOOL> playerhash;

inline void CheckProtocolVersion() {
#ifdef TARGET_BDS_PROTOCOL_VERSION
    auto currentProtocol = LL::getServerProtocolVersion();
    if (TARGET_BDS_PROTOCOL_VERSION != currentProtocol)
    {
        logger.warn("Protocol version not match, target version: {}, current version: {}.",
            TARGET_BDS_PROTOCOL_VERSION, currentProtocol);
        logger.warn("This will most likely crash the server, please use the Plugin that matches the BDS version!");
    }
#endif // TARGET_BDS_PROTOCOL_VERSION
}

void PluginInit()
{
    CheckProtocolVersion();
    auto updateServer = dlsym("?_updateServer@FishingHook@@IEAAXXZ");
    // 往后寻找 能判断鱼是否上钩的偏移
    for (int i = 0; i < 200; i++) {
        if (*reinterpret_cast<std::byte*>((intptr_t)updateServer + i) == (std::byte)0x89 && *reinterpret_cast<std::byte*>((intptr_t)updateServer + i + 1) == (std::byte)0x81) {
            fishinghook_offset = *reinterpret_cast<int*>((intptr_t)updateServer + i + 2);
            return;
        }
        else {
            i++;
        }
    }
    // 自动查找偏移失败 弹出提示 不进行下一步动作
    fishinghook_offset = -1;
    logger.warn("没能找到关键偏移,插件自动停用,请反馈给开发者并等待更新");
    logger.warn("https://github.com/cngege/LL_AutoFishing");
    
}

/// <summary>
/// 概率
/// </summary>
/// <param name="probability"></param>
/// <returns></returns>
bool Probability(float probability) {
    srand((unsigned)time(0));
    auto num = rand() % 101;
    auto IntProbability = probability * 100;
    if ((float)num <= IntProbability) {
        return true;
    }
    else {
        return false;
    }
}


/// <summary>
/// 获取鱼钩中鱼的咬钩进度
/// </summary>
/// <param name="fishingHook">鱼钩单例</param>
/// <returns>0:未上钩</returns>
int GetHookedTime(FishingHook* fishingHook) {
    if (fishinghook_offset <= 0) {
        return 0;
    }
    else {
        return *reinterpret_cast<int*>((intptr_t)fishingHook + fishinghook_offset);
    }
}


THook(void*, "?_hitCheck@FishingHook@@IEAA?AVHitResult@@XZ", FishingHook* thi,void* a1) {
    auto ret = original(thi, a1);
    int HookedTime = GetHookedTime(thi);
    if (fishinghook_offset > 0 && HookedTime > 0) {
        auto actor = thi->getOwner();
        if (actor->isPlayer()) {
            auto player = (Player*)actor;
            //thi->retrieve();        //直接调用这个函数的话会出BUG
            auto item = player->getHandSlot();
            item->use(*player);
            if (thi != nullptr) {
                thi->remove();
            }
            player->refreshInventory();

            // 设置标志位 间隔 0.5 - 1 秒后再次抛竿
            if(tickcount > 10)tickcount = 0;
            playerhash[player] = true;
        }
    }
    return ret;
}


THook(void, "?tickWorld@Player@@UEAAXAEBUTick@@@Z", Player* thi, void* tick) {
    if (fishinghook_offset <= 0) {
        return original(thi, tick);
    }
    if (tickcount < 20) {
        tickcount++;
    }
    else {
        if (playerhash[thi]) {
            //检测手中物品是否是鱼竿
            ItemStack* item = thi->getHandSlot();
            if (item->getTypeName() == "minecraft:fishing_rod") {
                item->use(*thi);
            }
            playerhash[thi] = false;
        }
        tickcount = 0;
    }
    original(thi, tick);
}