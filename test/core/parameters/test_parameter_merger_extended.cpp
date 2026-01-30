#include "LLMEngine/core/ParameterMerger.hpp"
#include "LLMEngine/http/RequestOptions.hpp"
#include <cassert>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace LLMEngine;

int main() {
    std::cout << "Testing ParameterMerger Extended Features...\n";

    // Test 1: Stream Options include_usage = true
    {
        RequestOptions opts;
        opts.stream_options = RequestOptions::StreamOptions{true};

        nlohmann::json params;
        ParameterMerger::mergeRequestOptions(params, opts);

        assert(params.contains("stream_options"));
        assert(params["stream_options"].contains("include_usage"));
        assert(params["stream_options"]["include_usage"] == true);
    }

    // Test 2: Stream Options include_usage = false
    {
        RequestOptions opts;
        opts.stream_options = RequestOptions::StreamOptions{false};

        nlohmann::json params;
        ParameterMerger::mergeRequestOptions(params, opts);

        assert(params.contains("stream_options"));
        assert(params["stream_options"].contains("include_usage"));
        assert(params["stream_options"]["include_usage"] == false);
    }

    // Test 3: No Stream Options
    {
        RequestOptions opts;
        // stream_options is nullopt

        nlohmann::json params;
        ParameterMerger::mergeRequestOptions(params, opts);

        assert(!params.contains("stream_options"));
    }

    std::cout << "ParameterMerger Extended tests passed.\n";
    return 0;
}
