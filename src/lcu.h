#pragma once

#include <string>
#include <array>
#include <map>

void init_lcu();

struct player
{
    std::string champion;
    std::string name;
    std::string team;
};

std::array<player, 10> get_players();

std::map<std::string, std::string> get_pros(const std::string& path);
