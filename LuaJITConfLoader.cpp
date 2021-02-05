// Copyright (c) 2020-2021 Nicholas Corgan
// SPDX-License-Identifier: MIT

#include "LuaJITBlock.hpp"

#include <Pothos/Callable.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Object.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Util/BlockDescription.hpp>

#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/StringTokenizer.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

struct FactoryArgs
{
    std::string factory;

    std::string sourceFilepath;
    std::string functionName;
    std::vector<std::string> inputTypes;
    std::vector<std::string> outputTypes;
};

static Pothos::Object opaqueLuaJITBlockFactory(
    const FactoryArgs& factoryArgs,
    const Pothos::Object* args,
    const size_t numArgs)
{
    auto blockPlugin = Pothos::PluginRegistry::get("/blocks/blocks/luajit_block");

    // The LuaJIT block takes in the input and output types, which are
    // provided by the configuration file. Theoretically, there should
    // be nothing extra passed in the args parameter, but incorporate
    // them anyway.
    Pothos::ObjectVector argsVector(args, args+numArgs);
    argsVector.emplace_back(factoryArgs.inputTypes);
    argsVector.emplace_back(factoryArgs.outputTypes);
    argsVector.emplace_back(false); // Disallow setting the source after construction

    // This backdoor allows us to create the block without allowing the
    // source to be set post-construction, then use our access to the
    // block type to call it via the function itself.
    auto callable = blockPlugin.getObject().extract<Pothos::Callable>();
    callable.unbind(2);

    auto luajitBlock = callable.opaqueCall(argsVector.data(), argsVector.size());

    luajitBlock.ref<Pothos::Block*>()->setName(factoryArgs.factory);

    // Pothos::Object::ref() doesn't allow pointer casts.
    dynamic_cast<LuaJITBlock*>(luajitBlock.ref<Pothos::Block*>())->setSource(
        factoryArgs.sourceFilepath,
        factoryArgs.functionName);

    // TODO: set preloaded libraries

    return luajitBlock;
}

static std::vector<std::string> stringTokenizerToVector(const Poco::StringTokenizer& tokenizer)
{
    std::vector<std::string> stdVector;
    std::copy(
        tokenizer.begin(),
        tokenizer.end(),
        std::back_inserter(stdVector));

    return stdVector;
}

static std::vector<Pothos::PluginPath> LuaJITConfLoader(const std::map<std::string, std::string>& config)
{
    static const auto tokOptions = Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM;
    static const std::string tokSep(" \t");

    FactoryArgs factoryArgs;

    // Set by calling function
    const auto confFilePathIter = config.find("confFilePath");
    if(confFilePathIter == config.end()) throw Pothos::Exception("No conf filepath");
    const auto rootDir = Poco::Path(confFilePathIter->second).makeParent();

    //
    // Factory parameters
    //

    auto factoryIter = config.find("factory");
    if(factoryIter != config.end())
    {
        // This will throw if the plugin path syntax is invalid.
        factoryArgs.factory = Pothos::PluginPath(factoryIter->second).toString();
    }
    else throw Pothos::Exception("No factory");

    // Policy: source must be a path
    auto sourceIter = config.find("source");
    if(sourceIter != config.end())
    {
        factoryArgs.sourceFilepath = Poco::Path(rootDir, sourceIter->second).toString();
        if(!Poco::File(factoryArgs.sourceFilepath).exists())
        {
            throw Pothos::FileNotFoundException(factoryArgs.sourceFilepath);
        }
    }
    else throw Pothos::Exception("No source");

    auto functionNameIter = config.find("function");
    if(functionNameIter != config.end()) factoryArgs.functionName = functionNameIter->second;
    else throw Pothos::Exception("No function name");

    auto inputTypesIter = config.find("input_types");
    if(inputTypesIter != config.end())
    {
        factoryArgs.inputTypes = stringTokenizerToVector(Poco::StringTokenizer(inputTypesIter->second, tokSep, tokOptions));
    }
    else throw Pothos::Exception("No input types");

    auto outputTypesIter = config.find("output_types");
    if(outputTypesIter != config.end())
    {
        factoryArgs.outputTypes = stringTokenizerToVector(Poco::StringTokenizer(outputTypesIter->second, tokSep, tokOptions));
    }
    else throw Pothos::Exception("No output types");

    // If the doc source isn't specified, use the source itself.
    std::string docSourceFilepath;
    auto docSourceIter = config.find("doc_source");
    if(docSourceIter != config.end())
    {
        docSourceFilepath = Poco::Path(rootDir, sourceIter->second).toString();
        if(!Poco::File(docSourceFilepath).exists())
        {
            throw Pothos::FileNotFoundException(docSourceFilepath);
        }

    }
    else docSourceFilepath = factoryArgs.sourceFilepath;

    Pothos::Util::BlockDescriptionParser parser;
    parser.feedFilePath(docSourceFilepath);

    //
    // Register all factory paths, using the parameters from the config file.
    //
    auto blockFactory = Pothos::Callable(&opaqueLuaJITBlockFactory)
                            .bind(factoryArgs, 0);
    Pothos::PluginRegistry::addCall(
        "/blocks"+factoryArgs.factory,
        blockFactory);
    Pothos::PluginRegistry::add(
        "/blocks/docs"+factoryArgs.factory,
        parser.getJSONObject(factoryArgs.factory));

    return
    {
        "/blocks"+factoryArgs.factory,
        "/blocks/docs"+factoryArgs.factory
    };
}

//
// Register conf loader
//
pothos_static_block(pothosRegisterLuaJITConfLoader)
{
    Pothos::PluginRegistry::addCall(
        "/framework/conf_loader/luajit",
        &LuaJITConfLoader);
}
