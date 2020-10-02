#define RAPIDJSON_ASSERT(x)
#include <FlowUtils/FlowJson.h>
#include <FlowUtils/FlowArgParser.h>
#include "FlowRender.h"
#include <chrono>


int main(int argc, char *argv[]) {
    FlowArgParser fap;
    fap.addIndexOption("template", "Template file", true);
    fap.addIndexOption("values", "Values as JSON", true);
    fap.parse(argc, argv);
    if(!fap.hasRequiredOptions()){
        std::cout << "missing parameters";
        return EXIT_FAILURE;
    }
    const auto tmplPath = fap.getString("template");
    const auto valuePath = fap.getString("values");

    const auto tmpl = FlowFile::fileToString(tmplPath);
    const auto values = FlowFile::fileToString(valuePath);
    const auto document = FlowJson::parseJson(values);
    FlowRender render;
    auto start = std::chrono::high_resolution_clock::now();
    const std::string result = render.render(tmpl, document, "../javatest");
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = finish - start;
    LOG_INFO << "Result:" << std::endl << result;
    LOG_INFO << "Duration: " << elapsed.count();
    FlowString::stringToFile("test.txt", result);


    return 0;
}
