#pragma once

#include <string>
#include <rapidjson/document.h>
#include <vector>
#include <algorithm>
#include <regex>
#include <sstream>
#include <FlowUtils/FlowLog.h>
#include <FlowUtils/FlowParser.h>
#include <FlowUtils/FlowJson.h>
#include "ListPosition.h"
#include "FlowRenderConst.h"

class FlowRender {

public:
    std::string render(const std::string &templ, const std::string &value,
                       const std::string &cwd = FlowFile::getCurrentDirectory()) {
        const auto doc = FlowJson::parseJson(value);
        return render(templ, doc, cwd);
    }

    std::string render(const std::string &templ, const rapidjson::Value &value) {
        const std::string cwd = FlowFile::getCurrentDirectory();
        return render(templ, value, cwd);
    }

    std::string render(const std::string &templ, const rapidjson::Value &value, const std::string &cwd) {
        std::stringstream rtn;
        size_t pos = 0;
        const auto matches = getAllMatches(templ);
        const rapidjson::Value &currentValue = value;
        std::vector<ListPosition> listPositions;

        for (size_t i = 0; i < matches.size(); ++i) {
            const auto &match = matches[i];
            const auto part = templ.substr(pos, match.position() - pos);
            rtn << part;
            pos = match.position() + match.length();
            std::string resolved_string = resolve(currentValue, match, pos, templ, listPositions, i, cwd, matches);
            rtn << resolved_string;
        }
        if (pos < templ.length())
            rtn << templ.substr(pos);
        return rtn.str();
    }

private:
    std::string resolveValuePath(const std::string &valuePath, const std::vector<ListPosition> &listPositions) {
        if (!valuePath.empty() && valuePath.at(0) == '@') {
            if (listPositions.empty())
                return valuePath.substr(1);

            const auto &last = listPositions.back();
            FlowJson::normelizePath(last.listPath, valuePath.substr(1));
        }

        if (listPositions.empty())
            return valuePath;
        const auto &last = listPositions.back();
        const auto prefix = last.listPath.substr(0, last.listPath.length() - 1) + std::to_string(last.listPos);
        if (valuePath == ".")
            return prefix;
        return prefix + valuePath;

    }

    void forwardListPosition(std::vector<ListPosition> &listPositions, size_t &matchPosition, size_t &pos) {
        if (listPositions.empty())
            return;

        auto &last = listPositions.back();
        if (++last.listPos >= last.listLength) {
            listPositions.pop_back();
        } else {
            matchPosition = listPositions.back().listStartMatch;
            pos = last.listStartPosition;
        }
    }

    bool lastInList(const std::vector<ListPosition> &listPosition) {
        const auto &last = listPosition.back();
        return (last.listPos + 1) == last.listLength;
    }

    bool firstInList(const std::vector<ListPosition> &listPosition) {
        const auto &last = listPosition.back();
        return last.listPos == 0;
    }

    void gotoNextEnd(const std::vector<std::smatch> &matches, size_t &matchPosition, size_t &pos) {
        while (++matchPosition < matches.size()) {
            const auto type = matches[matchPosition][1];
            std::string value = matches[matchPosition][2];
            FlowString::trim(value);
            if (type == "c" && value == "end") {
                pos = matches[matchPosition].position();
                --matchPosition;
                return;
            }
            if (type == "e" && value == "end") {
                pos = matches[matchPosition].position();
                --matchPosition;
                return;
            }
            if (type == "l" && value == "end") {
                --matchPosition;
                pos = matches[matchPosition].position() + matches[matchPosition].length();
                return;
            }
            if (type == "h" && value == "end") {
                --matchPosition;
                pos = matches[matchPosition].position() + matches[matchPosition].length();
                return;
            }
        }
    }

    void gotoNextEnd(const std::vector<std::smatch> &matches, size_t &matchPosition, size_t &pos,
                     const std::string &end_type) {
        while (++matchPosition < matches.size()) {
            const auto type = matches[matchPosition][1];
            std::string value = matches[matchPosition][2];
            FlowString::trim(value);
            if (type == end_type && value == "end") {
                pos = matches[matchPosition].position();
                --matchPosition;
                return;
            }
        }
    }

    std::string
    resolve(const rapidjson::Value &value, const std::smatch &match, size_t &pos, const std::string &templ,
            std::vector<ListPosition> &listPositions, size_t &matchPosition, const std::string &cwd,
            const std::vector<std::smatch> &matches) {
        std::string rtn;
        const auto type = match[1];
        if (type == "v") {
            const std::string valuePath = match[2];
            const std::string absolutePath = resolveValuePath(valuePath, listPositions);
            rtn = FlowJson::getValue(value, absolutePath);
        } else if (type == "i") {
            FlowParser::goToNextLine(templ, pos);
            std::string filePath = match[2];
            FlowString::trim(filePath);
            const auto path = FlowFile::combinePath(cwd, filePath);
            const auto iTempl = FlowFile::fileToString(path);
            const auto icwd = FlowFile::getParentPath(path);
            const std::string absolutePath = resolveValuePath("", listPositions);
            const rapidjson::Value &iValue = (*FlowJson::getJValue(value, absolutePath));
            rtn = render(iTempl, iValue, icwd);
        } else if (type == "l") {
            FlowParser::goToNextLine(templ, pos);
            std::string valuePath = match[2];
            FlowString::trim(valuePath);
            if (valuePath == "end") {
                forwardListPosition(listPositions, matchPosition, pos);
            } else {
                const std::string absolutePath = resolveValuePath(valuePath, listPositions);
                ListPosition &listPosition = listPositions.emplace_back();
                listPosition.listStartMatch = matchPosition;
                listPosition.listPath = absolutePath;
                listPosition.listPos = 0;
                listPosition.listStartPosition = pos;
                const auto value_paths = FlowJson::findAllJsonValues(value, listPosition.listPath);
                listPosition.listLength = value_paths.size();
                if (listPositions.back().listLength == 0) {
                    gotoNextEnd(matches, matchPosition, pos, "l");
                    FlowParser::goToNextLine(templ, pos);
                }
            }
        } else if (type == "f") {
            const std::string match_value = match[2];
            const auto split_value = FlowString::splitOnFirst(match_value, " ");
            auto functionName = split_value[0];
            FlowString::trim(functionName);
            if (functionName != "end") {
                if (functionName == "listInfo") {
                    const std::string absolutePath = resolveValuePath("", listPositions);
                    rtn = absolutePath;
                } else if (functionName == "list") {
                    auto value_path = split_value[1];
                    FlowString::trim(value_path);
                    const auto doc_values = FlowJson::getValues(value, value_path);
                    rtn = FlowString::join(doc_values, "\n");
                } else if (functionName == "not_last") {
                    if (!lastInList(listPositions))
                        rtn = split_value[1];
                } else if (functionName == "last") {
                    if (lastInList(listPositions))
                        rtn = split_value[1];
                } else if (functionName == "not_first") {
                    if (!firstInList(listPositions))
                        rtn = split_value[1];
                } else if (functionName == "join") {
                    auto value_path = split_value[1];
                    FlowString::trim(value_path);
                    auto delimiter = split_value[2];
                    const auto doc_values = FlowJson::getValues(value, value_path);
                    rtn = FlowString::join(doc_values, delimiter);
                } else if (functionName == "upper") {
                    auto valuePath = split_value[1];
                    FlowString::trim(valuePath);
                    const std::string absolutePath = resolveValuePath(valuePath, listPositions);
                    rtn = FlowJson::getValue(value, absolutePath);
                    FlowString::toUpper(rtn);
                } else if (functionName == "lower") {
                    auto valuePath = split_value[1];
                    FlowString::trim(valuePath);
                    const std::string absolutePath = resolveValuePath(valuePath, listPositions);
                    rtn = FlowJson::getValue(value, absolutePath);
                    FlowString::toLower(rtn);
                }
            }
        } else if (type == "r") {
            rtn = match[2];
        } else if (type == "e") {
            FlowParser::goToNextLine(templ, pos);
            std::string values = match[2];
            FlowString::trim(values);
            if (values != "end") {
                const auto valuesVector = FlowString::splitNotEmpty(values, " ");
                if (valuesVector.size() != 2)
                    return "";

                const auto path = valuesVector[0];
                const auto equalsTo = valuesVector[1];

                const std::string absolutePath = resolveValuePath(path, listPositions);
                const auto listPath = resolveValuePath("", listPositions);
                const auto value_paths = FlowJson::getValue(value, absolutePath);
                if (value_paths != equalsTo) {
                    gotoNextEnd(matches, matchPosition, pos, "e");
                }
            }
        } else if (type == "c") {
            FlowParser::goToNextLine(templ, pos);
            std::string values = match[2];
            FlowString::trim(values);
            if (values == "end") {
            } else {
                const auto valuesVector = FlowString::splitOnFirst(values, " ");
                if (valuesVector.size() != 2)
                    return "";

                const auto path = valuesVector[0];
                const auto equalsTo = valuesVector[1];

                const std::string absolutePath = resolveValuePath(path, listPositions);
                const auto listPath = resolveValuePath("", listPositions);
                const auto value_paths = FlowJson::getValue(value, absolutePath);
                if (!checkRegex(value_paths, equalsTo)) {
                    gotoNextEnd(matches, matchPosition, pos, "c");
                }
            }
        } else if (type == "h") {
            FlowParser::goToNextLine(templ, pos);
            std::string values = match[2];
            FlowString::trim(values);
            if (values == "end") {
            } else {
                const auto valuesVector = FlowString::splitNotEmpty(values, " ");
                if (valuesVector.size() != 1)
                    return "";

                const auto path = valuesVector[0];

                const std::string absolutePath = resolveValuePath(path, listPositions);
                const auto listPath = resolveValuePath("", listPositions);
                const auto exists = FlowJson::exists(value, absolutePath);
                if (!exists) {
                    gotoNextEnd(matches, matchPosition, pos, "h");
                }
            }
        } else if (type == "n") {
            FlowParser::goToNextLine(templ, pos);
            std::string values = match[2];
            FlowString::trim(values);
            if (values != "end") {
                const auto valuesVector = FlowString::splitOnFirst(values, " ");
                if (valuesVector.empty())
                    return "";

                const auto &path = valuesVector[0];
                const auto &checkToPath = valuesVector.size() == 2 ? valuesVector[1] : "";
                const std::string absolutePath = resolveValuePath(path, listPositions);
                const auto listPath = resolveValuePath("", listPositions);
                const auto pathValue = FlowJson::getValue(value, absolutePath);
                if (!pathValue.empty() && (checkToPath.empty() || checkToPath == pathValue)) {
                    gotoNextEnd(matches, matchPosition, pos, "n");
                }
            }
        }

        return rtn;
    }

    static bool checkRegex(const std::string &value, const std::string &expr) {
        std::regex to_match(expr);
        return std::regex_match(value, to_match);
    }

    static std::vector<std::smatch> getAllMatches(const std::string &templ) {
        std::vector<std::smatch> rtn;
        auto start = std::sregex_iterator(templ.begin(), templ.end(), FlowRenderConst::MAIN_FIND);
        auto end = std::sregex_iterator();
        for (; start != end; ++start) {
            rtn.emplace_back(*start);
        }
        return rtn;
    }


};