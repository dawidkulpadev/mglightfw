/**
    MioGiapicco Light Firmware - Firmware for Light Device of MioGiapicco system
    Copyright (C) 2026  Dawid Kulpa

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Please feel free to contact me at any time by email <dawidkulpadev@gmail.com>
*/

#ifndef SUPERVISOR_SUPERSTRING_H
#define SUPERVISOR_SUPERSTRING_H


#include <vector>
#include <string>

typedef std::vector<std::string> StringList;

StringList split(const std::string &s, char d);
StringList splitCsvRespectingQuotes(const std::string& s, char delim=',');
void removeLeading(std::string &s, const std::string &l);
void removeLeading(std::string &s, char c);
void removeTrailing(std::string &s, const std::string &l);
void removeTrailing(std::string &s, char c);



#endif //SUPERVISOR_SUPERSTRING_H
