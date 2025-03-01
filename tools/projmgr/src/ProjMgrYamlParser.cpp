/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrYamlParser.h"
#include "ProjMgrLogger.h"
#include "ProjMgrYamlSchemaChecker.h"

#include "RteFsUtils.h"
#include <string>

using namespace std;

ProjMgrYamlParser::ProjMgrYamlParser(void) {
  // Reserved
}

ProjMgrYamlParser::~ProjMgrYamlParser(void) {
  // Reserved
}

bool ProjMgrYamlParser::ParseCdefault(const string& input,
  CdefaultItem& cdefault, bool checkSchema) {
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(
        input, ProjMgrYamlSchemaChecker::FileType::DEFAULT)) {
      return false;
    }

    cdefault.path = RteFsUtils::MakePathCanonical(input);

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCdefault(input, root)) {
      return false;
    }

    const YAML::Node& defaultNode = root[YAML_DEFAULT];
    ParseBuildTypes(defaultNode, cdefault.buildTypes);
    ParseString(defaultNode, YAML_COMPILER, cdefault.compiler);
    ParsePacks(defaultNode, cdefault.packs);
  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseCsolution(const string& input,
  CsolutionItem& csolution, bool checkSchema) {
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(
        input, ProjMgrYamlSchemaChecker::FileType::SOLUTION)) {
      return false;
    }

    csolution.path = RteFsUtils::MakePathCanonical(input);
    csolution.directory = RteFsUtils::ParentPath(csolution.path);

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCsolution(input, root)) {
      return false;
    }

    const YAML::Node& solutionNode = root[YAML_SOLUTION];
    if (!ParseContexts(solutionNode, csolution)) {
      return false;
    }

    ParseTargetTypes(solutionNode, csolution.targetTypes);
    ParseBuildTypes(solutionNode, csolution.buildTypes);
    ParseOutputDirs(solutionNode, csolution.directories);
    ParseTargetType(solutionNode, csolution.target);
    ParsePacks(solutionNode, csolution.packs);

  } catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseCproject(const string& input,
  CsolutionItem& csolution, map<string, CprojectItem>& cprojects,
  bool single, bool checkSchema) {
  CprojectItem cproject;
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(
        input, ProjMgrYamlSchemaChecker::FileType::PROJECT)) {
      return false;
    }

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCproject(input, root)) {
      return false;
    }

    cproject.path = RteFsUtils::MakePathCanonical(input);
    cproject.directory = RteFsUtils::ParentPath(cproject.path);
    cproject.name = fs::path(input).stem().stem().generic_string();

    const YAML::Node& projectNode = root[YAML_PROJECT];
    map<const string, string&> projectChildren = {
      {YAML_OUTPUTTYPE, cproject.outputType},
    };
    for (const auto& item : projectChildren) {
      ParseString(projectNode, item.first, item.second);
    }
    ParseTargetType(projectNode, cproject.target);

    if (!ParseComponents(projectNode, cproject.components)) {
      return false;
    }
    if (!ParseGroups(projectNode, cproject.groups)) {
      return false;
    }
    if (!ParseLayers(projectNode, cproject.clayers)) {
      return false;
    }
    if (!ParseSetups(projectNode, cproject.setups)) {
      return false;
    }

  } catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }

  cprojects[input] = cproject;

  if (single) {
    csolution.directory = cproject.directory;
    csolution.contexts.push_back({ input });
  }
  return true;
}

bool ProjMgrYamlParser::ParseClayer(const string& input,
  map<string, ClayerItem>& clayers, bool checkSchema) {
  ClayerItem clayer;
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(
        input, ProjMgrYamlSchemaChecker::FileType::LAYER)) {
      return false;
    }

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateClayer(input, root)) {
      return false;
    }

    clayer.path = RteFsUtils::MakePathCanonical(input);
    clayer.directory = RteFsUtils::ParentPath(clayer.path);
    clayer.name = fs::path(input).stem().stem().generic_string();

    const YAML::Node& layerNode = root[YAML_LAYER];
    map<const string, string&> projectChildren = {
      {YAML_OUTPUTTYPE, clayer.outputType},
    };
    for (const auto& item : projectChildren) {
      ParseString(layerNode, item.first, item.second);
    }

    ParseTargetType(layerNode, clayer.target);

    if (!ParseComponents(layerNode, clayer.components)) {
      return false;
    }
    if (!ParseGroups(layerNode, clayer.groups)) {
      return false;
    }
    //TODO: Parse Interfaces

  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  clayers[input] = clayer;
  return true;
}

void ProjMgrYamlParser::ParseString(const YAML::Node& parent, const string& key, string& value) {
  if (parent[key].IsDefined()) {
    value = parent[key].as<string>();
    if (parent[key].Type() == YAML::NodeType::Null) {
      value = "";
    }
  }
}

void ProjMgrYamlParser::ParseVector(const YAML::Node& parent, const string& key, vector<string>& value) {
  if (parent[key].IsDefined() && parent[key].IsSequence()) {
    value = parent[key].as<vector<string>>();
  }
}

void ProjMgrYamlParser::ParseVectorOrString(const YAML::Node& parent, const string& key, vector<string>& value) {
  ParseVector(parent, key, value);
  if (value.empty()) {
    string strValue;
    ParseString(parent, key, strValue);
    if (!strValue.empty()) {
      value.push_back({ strValue });
    }
  }
}

void ProjMgrYamlParser::ParseProcessor(const YAML::Node& parent, ProcessorItem& processor) {
  if (parent[YAML_PROCESSOR].IsDefined()) {
    const YAML::Node& processorNode = parent[YAML_PROCESSOR];
    map<const string, string&> processorChildren = {
      {YAML_TRUSTZONE, processor.trustzone},
      {YAML_FPU, processor.fpu},
      {YAML_ENDIAN, processor.endian},
    };
    for (const auto& item : processorChildren) {
      ParseString(processorNode, item.first, item.second);
    }
  }
}

bool ProjMgrYamlParser::ParseComponents(const YAML::Node& parent, vector<ComponentItem>& components) {
  if (parent[YAML_COMPONENTS].IsDefined()) {
    const YAML::Node& componentsNode = parent[YAML_COMPONENTS];
    for (const auto& componentEntry : componentsNode) {
      ComponentItem componentItem;
      if (!ParseTypeFilter(componentEntry, componentItem.type)) {
        return false;
      }
      ParseString(componentEntry, YAML_COMPONENT, componentItem.component);
      ParseBuildType(componentEntry, componentItem.build);
      components.push_back(componentItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseTypeFilter(const YAML::Node& parent, TypeFilter& type) {
  vector<string> include, exclude;
  ParseVectorOrString(parent, YAML_FORTYPE, include);
  ParseVectorOrString(parent, YAML_NOTFORTYPE, exclude);
  if (!ParseTypePair(include, type.include) ||
      !ParseTypePair(exclude, type.exclude)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseTypePair(vector<string>& vec, vector<TypePair>& typeVec) {
  bool valid = true;
  for (const auto& item : vec) {
    TypePair typePair;
    if (!GetTypes(item, typePair.build, typePair.target)) {
      valid = false;
    }
    typeVec.push_back(typePair);
  }
  return valid;
}

bool ProjMgrYamlParser::GetTypes(const string& type, string& buildType, string& targetType) {
  size_t buildDelimiter = type.find(".");
  size_t targetDelimiter = type.find("+");
  if (((buildDelimiter > 0) && (targetDelimiter > 0)) || (buildDelimiter == targetDelimiter)) {
    ProjMgrLogger::Error("type '" + type + "': delimiters '.' or '+' not set)");
    return false;
  }
  if (targetDelimiter > buildDelimiter) {
    if (targetDelimiter < string::npos) {
      targetType = type.substr(targetDelimiter + 1, string::npos);
      targetDelimiter--;
    }
    buildType = type.substr(buildDelimiter + 1, targetDelimiter);
  }
  else if (buildDelimiter > targetDelimiter) {
    if (buildDelimiter < string::npos) {
      buildType = type.substr(buildDelimiter + 1, string::npos);
      buildDelimiter--;
    }
    targetType = type.substr(targetDelimiter + 1, buildDelimiter);
  }
  return true;
}

void ProjMgrYamlParser::ParseMisc(const YAML::Node& parent, vector<MiscItem>& misc) {
  if (parent[YAML_MISC].IsDefined()) {
    const YAML::Node& miscNode = parent[YAML_MISC];
    for (const auto& miscEntry : miscNode) {
      MiscItem miscItem;
      ParseString(miscEntry, YAML_FORCOMPILER, miscItem.compiler);
      if (miscItem.compiler.empty()) {
        // TODO: after deprecation remove 'compiler' keyword parsing in benefit of 'for-compiler'
        ParseString(miscEntry, YAML_COMPILER, miscItem.compiler);
      }
      ParseVector(miscEntry, YAML_MISC_C, miscItem.c);
      ParseVector(miscEntry, YAML_MISC_CPP, miscItem.cpp);
      ParseVector(miscEntry, YAML_MISC_C_CPP, miscItem.c_cpp);
      if (miscItem.c_cpp.empty()) {
        // TODO: after deprecation remove 'C*' keyword parsing in benefit of 'C-CPP'
        ParseVector(parent, YAML_MISC_C_STAR, miscItem.c_cpp);
      }
      ParseVector(miscEntry, YAML_MISC_ASM, miscItem.as);
      ParseVector(miscEntry, YAML_MISC_LINK, miscItem.link);
      ParseVector(miscEntry, YAML_MISC_LIB, miscItem.lib);
      misc.push_back(miscItem);
    }
  }
}

void ProjMgrYamlParser::ParsePacks(const YAML::Node& parent, vector<PackItem>& packs) {
  if (parent[YAML_PACKS].IsDefined()) {
    const YAML::Node& packNode = parent[YAML_PACKS];
    for (const auto& packEntry : packNode) {
      PackItem packItem;
      ParseString(packEntry, YAML_PACK, packItem.pack);
      ParseString(packEntry, YAML_PATH, packItem.path);
      ParseTypeFilter(packEntry, packItem.type);
      packs.push_back(packItem);
    }
  }
}

bool ProjMgrYamlParser::ParseFiles(const YAML::Node& parent, vector<FileNode>& files) {
  if (parent[YAML_FILES].IsDefined()) {
    const YAML::Node& filesNode = parent[YAML_FILES];
    for (const auto& fileEntry : filesNode) {
      FileNode fileItem;
      if (!ParseTypeFilter(fileEntry, fileItem.type)) {
        return false;
      }
      ParseString(fileEntry, YAML_FILE, fileItem.file);
      ParseVectorOrString(fileEntry, YAML_FORCOMPILER, fileItem.forCompiler);
      ParseString(fileEntry, YAML_CATEGORY, fileItem.category);
      ParseBuildType(fileEntry, fileItem.build);
      files.push_back(fileItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseGroups(const YAML::Node& parent, vector<GroupNode>& groups) {
  if (parent[YAML_GROUPS].IsDefined()) {
    const YAML::Node& groupsNode = parent[YAML_GROUPS];
    for (const auto& groupEntry : groupsNode) {
      GroupNode groupItem;
      if (!ParseTypeFilter(groupEntry, groupItem.type)) {
        return false;
      }
      if (!ParseFiles(groupEntry, groupItem.files)) {
        return false;
      }
      ParseString(groupEntry, YAML_GROUP, groupItem.group);
      ParseVectorOrString(groupEntry, YAML_FORCOMPILER, groupItem.forCompiler);
      ParseBuildType(groupEntry, groupItem.build);
      ParseGroups(groupEntry, groupItem.groups);
      groups.push_back(groupItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseLayers(const YAML::Node& parent, vector<LayerItem>& layers) {
  if (parent[YAML_LAYERS].IsDefined()) {
    const YAML::Node& layersNode = parent[YAML_LAYERS];
    for (const auto& layerEntry : layersNode) {
      LayerItem layerItem;
      if (!ParseTypeFilter(layerEntry, layerItem.type)) {
        return false;
      }
      ParseString(layerEntry, YAML_LAYER, layerItem.layer);
      ParseBuildType(layerEntry, layerItem.build);
      layers.push_back(layerItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseSetups(const YAML::Node& parent, vector<SetupItem>& setups) {
  if (parent[YAML_SETUPS].IsDefined()) {
    const YAML::Node& setupsNode = parent[YAML_SETUPS];
    for (const auto& setupEntry : setupsNode) {
      SetupItem setupItem;
      if (!ParseTypeFilter(setupEntry, setupItem.type)) {
        return false;
      }
      ParseString(setupEntry, YAML_SETUP, setupItem.description);
      ParseVectorOrString(setupEntry, YAML_FORCOMPILER, setupItem.forCompiler);
      ParseBuildType(setupEntry, setupItem.build);
      setups.push_back(setupItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseContexts(const YAML::Node& parent, CsolutionItem& csolution) {
  if (parent[YAML_PROJECTS].IsDefined()) {
    const YAML::Node& projectsNode = parent[YAML_PROJECTS];
    for (const auto& projectsEntry : projectsNode) {
      ContextDesc descriptor;
      if (!ParseTypeFilter(projectsEntry, descriptor.type)) {
        return false;
      }
      ParseString(projectsEntry, YAML_PROJECT, descriptor.cproject);
      csolution.contexts.push_back(descriptor);
      PushBackUniquely(csolution.cprojects, descriptor.cproject);
    }
  }
  return true;
}

void ProjMgrYamlParser::ParseBuildTypes(const YAML::Node& parent, map<string, BuildType>& buildTypes) {
  if (parent[YAML_BUILDTYPES].IsDefined()) {
    const YAML::Node& buildTypesNode = parent[YAML_BUILDTYPES];
    for (const auto& typeEntry : buildTypesNode) {
      string typeItem;
      BuildType build;
      ParseString(typeEntry, YAML_TYPE, typeItem);
      ParseBuildType(typeEntry, build);
      buildTypes[typeItem] = build;
    }
  }
}

void ProjMgrYamlParser::ParseOutputDirs(const YAML::Node& parent, struct DirectoriesItem& directories) {
  if (parent[YAML_OUTPUTDIRS].IsDefined()) {
    const YAML::Node& outputDirsNode = parent[YAML_OUTPUTDIRS];
    map<const string, string&> outputDirsChildren = {
      {YAML_OUTPUT_CPRJDIR, directories.cprj},
      {YAML_OUTPUT_INTDIR, directories.intdir},
      {YAML_OUTPUT_OUTDIR, directories.outdir},
      {YAML_OUTPUT_RTEDIR, directories.rte},
    };
    for (const auto& item : outputDirsChildren) {
      ParseString(outputDirsNode, item.first, item.second);
    }
  }
}

void ProjMgrYamlParser::ParseTargetTypes(const YAML::Node& parent, map<string, TargetType>& targetTypes) {
  if (parent[YAML_TARGETTYPES].IsDefined()) {
    const YAML::Node& targetTypesNode = parent[YAML_TARGETTYPES];
    for (const auto& typeEntry : targetTypesNode) {
      string typeItem;
      TargetType target;
      ParseString(typeEntry, YAML_TYPE, typeItem);
      ParseTargetType(typeEntry, target);
      targetTypes[typeItem] = target;
    }
  }
}

void ProjMgrYamlParser::ParseBuildType(const YAML::Node& parent, BuildType& buildType) {
  map<const string, string&> buildChildren = {
    {YAML_COMPILER, buildType.compiler},
    {YAML_OPTIMIZE, buildType.optimize},
    {YAML_DEBUG, buildType.debug},
    {YAML_WARNINGS, buildType.debug},
  };
  for (const auto& item : buildChildren) {
    ParseString(parent, item.first, item.second);
  }
  ParseProcessor(parent, buildType.processor);
  ParseMisc(parent, buildType.misc);
  ParseVector(parent, YAML_DEFINE, buildType.defines);
  if (buildType.defines.empty()) {
    // TODO: after deprecation remove 'defines' keyword parsing in benefit of 'define'
    ParseVector(parent, YAML_DEFINES, buildType.defines);
  }
  ParseVector(parent, YAML_UNDEFINE, buildType.undefines);
  if (buildType.undefines.empty()) {
    // TODO: after deprecation remove 'undefines' keyword parsing in benefit of 'undefine'
    ParseVector(parent, YAML_UNDEFINES, buildType.undefines);
  }
  ParseVector(parent, YAML_ADDPATH, buildType.addpaths);
  if (buildType.addpaths.empty()) {
    // TODO: after deprecation remove 'add-paths' keyword parsing in benefit of 'add-path'
    ParseVector(parent, YAML_ADDPATHS, buildType.addpaths);
  }
  ParseVector(parent, YAML_DELPATH, buildType.delpaths);
  if (buildType.delpaths.empty()) {
    // TODO: after deprecation remove 'del-paths' keyword parsing in benefit of 'del-path'
    ParseVector(parent, YAML_DELPATHS, buildType.delpaths);
  }
}

void ProjMgrYamlParser::ParseTargetType(const YAML::Node& parent, TargetType& targetType) {
  map<const string, string&> targetChildren = {
    {YAML_BOARD, targetType.board},
    {YAML_DEVICE, targetType.device},
  };
  for (const auto& item : targetChildren) {
    ParseString(parent, item.first, item.second);
  }
  ParseBuildType(parent, targetType.build);
}

void ProjMgrYamlParser::PushBackUniquely(vector<string>& vec, const string& value) {
  if (find(vec.cbegin(), vec.cend(), value) == vec.cend()) {
    vec.push_back(value);
  }
}

// Validation Maps
const set<string> defaultKeys = {
  YAML_BUILDTYPES,
  YAML_COMPILER,
  YAML_PACKS,
};

const set<string> solutionKeys = {
  YAML_PROJECTS,
  YAML_TARGETTYPES,
  YAML_BUILDTYPES,
  YAML_OUTPUTDIRS,
  YAML_PACKS,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_DEFINE,
  YAML_UNDEFINES,
  YAML_UNDEFINE,
  YAML_ADDPATHS,
  YAML_ADDPATH,
  YAML_DELPATHS,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> projectsKeys = {
  YAML_PROJECT,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
};

const set<string> projectKeys = {
  YAML_DESCRIPTION,
  YAML_OUTPUTTYPE,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_DEFINE,
  YAML_UNDEFINES,
  YAML_UNDEFINE,
  YAML_ADDPATHS,
  YAML_ADDPATH,
  YAML_DELPATHS,
  YAML_DELPATH,
  YAML_MISC,
  YAML_COMPONENTS,
  YAML_GROUPS,
  YAML_LAYERS,
  YAML_SETUPS,
};

const set<string> layerKeys = {
  YAML_DESCRIPTION,
  YAML_OUTPUTTYPE,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_DEFINE,
  YAML_UNDEFINES,
  YAML_UNDEFINE,
  YAML_ADDPATHS,
  YAML_ADDPATH,
  YAML_DELPATHS,
  YAML_DELPATH,
  YAML_MISC,
  YAML_COMPONENTS,
  YAML_GROUPS,
  YAML_INTERFACES,
};

const set<string> targetTypeKeys = {
  YAML_TYPE,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_DEFINE,
  YAML_UNDEFINES,
  YAML_UNDEFINE,
  YAML_ADDPATHS,
  YAML_ADDPATH,
  YAML_DELPATHS,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> buildTypeKeys = {
  YAML_TYPE,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_DEFINE,
  YAML_UNDEFINES,
  YAML_UNDEFINE,
  YAML_ADDPATHS,
  YAML_ADDPATH,
  YAML_DELPATHS,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> outputDirsKeys = {
  YAML_OUTPUT_CPRJDIR,
  YAML_OUTPUT_INTDIR,
  YAML_OUTPUT_OUTDIR,
  YAML_OUTPUT_RTEDIR,
};

const set<string> processorKeys = {
  YAML_TRUSTZONE,
  YAML_FPU,
  YAML_ENDIAN,
};

const set<string> miscKeys = {
  YAML_COMPILER,
  YAML_FORCOMPILER,
  YAML_MISC_C,
  YAML_MISC_CPP,
  YAML_MISC_C_STAR,
  YAML_MISC_C_CPP,
  YAML_MISC_ASM,
  YAML_MISC_LINK,
  YAML_MISC_LIB,
};

const set<string> packsKeys = {
  YAML_PACK,
  YAML_PATH,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
};

const set<string> componentsKeys = {
  YAML_COMPONENT,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_DEFINE,
  YAML_UNDEFINES,
  YAML_UNDEFINE,
  YAML_ADDPATHS,
  YAML_ADDPATH,
  YAML_DELPATHS,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> layersKeys = {
  YAML_LAYER,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_DEFINE,
  YAML_UNDEFINES,
  YAML_UNDEFINE,
  YAML_ADDPATHS,
  YAML_ADDPATH,
  YAML_DELPATHS,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> interfacesKeys = {
  YAML_PROVIDES,
  YAML_CONSUMES
};

const set<string> groupsKeys = {
  YAML_GROUP,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
  YAML_FORCOMPILER,
  YAML_GROUPS,
  YAML_FILES,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_DEFINE,
  YAML_UNDEFINES,
  YAML_UNDEFINE,
  YAML_ADDPATHS,
  YAML_ADDPATH,
  YAML_DELPATHS,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> filesKeys = {
  YAML_FILE,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
  YAML_FORCOMPILER,
  YAML_CATEGORY,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_DEFINE,
  YAML_UNDEFINES,
  YAML_UNDEFINE,
  YAML_ADDPATHS,
  YAML_ADDPATH,
  YAML_DELPATHS,
  YAML_DELPATH,
  YAML_MISC,
};

const map<string, set<string>> sequences = {
  {YAML_PROJECTS, projectsKeys},
  {YAML_TARGETTYPES, targetTypeKeys},
  {YAML_BUILDTYPES, buildTypeKeys},
  {YAML_MISC, miscKeys},
  {YAML_PACKS, packsKeys},
  {YAML_COMPONENTS, componentsKeys},
  {YAML_LAYERS, layersKeys},
  {YAML_GROUPS, groupsKeys},
  {YAML_FILES, filesKeys},
  {YAML_INTERFACES, interfacesKeys},
};

const map<string, set<string>> mappings = {
  {YAML_PROCESSOR, processorKeys},
  {YAML_OUTPUTDIRS, outputDirsKeys},
};

bool ProjMgrYamlParser::ValidateCdefault(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_DEFAULT,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& defaultNode = root[YAML_DEFAULT];
  if (!ValidateKeys(input, defaultNode, defaultKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateCsolution(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_SOLUTION,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& solutionNode = root[YAML_SOLUTION];
  if (!ValidateKeys(input, solutionNode, solutionKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateCproject(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_PROJECT,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& projectNode = root[YAML_PROJECT];
  if (!ValidateKeys(input, projectNode, projectKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateClayer(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_LAYER,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& layerNode = root[YAML_LAYER];
  if (!ValidateKeys(input, layerNode, layerKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateKeys(const string& input, const YAML::Node& parent, const set<string>& keys) {
  bool valid = true;
  for (const auto& item : parent) {
    const string& key = item.first.as<string>();
    if (keys.find(item.first.as<string>()) == keys.end()) {
      ProjMgrLogger::Warn(input, item.first.Mark().line + 1, item.first.Mark().column + 1, "key '" + item.first.as<string>() + "' was not recognized");
    }
    if (item.second.IsSequence()) {
      if (!ValidateSequence(input, item.second, key)) {
        valid = false;
      }
    } else if (sequences.find(key) != sequences.end()) {
      ProjMgrLogger::Error(input, item.first.Mark().line + 1, item.first.Mark().column + 1, "node '" + item.first.as<string>() + "' shall contain sequence elements");
      valid = false;
    }
    if (item.second.IsMap()) {
      if (!ValidateMapping(input, item.second, key)) {
        valid = false;
      }
    } else if (mappings.find(key) != mappings.end()) {
      ProjMgrLogger::Error(input, item.first.Mark().line + 1, item.first.Mark().column + 1, "node '" + item.first.as<string>() + "' shall contain mapping elements");
      valid = false;
    }
  }
  return valid;
}

bool ProjMgrYamlParser::ValidateSequence(const string& input, const YAML::Node& parent, const string& key) {
  bool valid = true;
  if (sequences.find(key) != sequences.end()) {
    set<string> keys = sequences.at(key);
    for (const YAML::Node& entry : parent) {
      if (!ValidateKeys(input, entry, keys)) {
        valid = false;
      }
    }
  }
  return valid;
}

bool ProjMgrYamlParser::ValidateMapping(const string& input, const YAML::Node& parent, const string& key) {
  bool valid = true;
  if (mappings.find(key) != mappings.end()) {
    set<string> keys = mappings.at(key);
    if (!ValidateKeys(input, parent, keys)) {
      valid = false;
    }
  }
  return valid;
}
