/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCondition.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteCondition.h"

#include "RtePackage.h"
#include "RteModel.h"

#include "XMLTree.h"

using namespace std;

RteConditionExpression::RteConditionExpression(RteCondition* parent) :
  RteItem(parent),
  m_domain(0)
{
};

RteConditionExpression::~RteConditionExpression()
{
}

bool RteConditionExpression::IsDenyExpression() const {
  return GetExpressionType() == DENY;
}

bool RteConditionExpression::IsDependencyExpression() const
{
  return m_domain == COMPONENT_EXPRESSION;
}

bool RteConditionExpression::IsDeviceExpression() const
{
  return m_domain == DEVICE_EXPRESSION;
}

bool RteConditionExpression::IsDeviceDependent() const
{
  if (IsDeviceExpression()) {
    return !GetAttribute("Dname").empty();
  }
  if (GetExpressionDomain() == CONDITION_EXPRESSION) {
    return RteItem::IsDeviceDependent();
  }
  return false;
}

bool RteConditionExpression::IsBoardExpression() const
{
  return m_domain == BOARD_EXPRESSION;
}

bool RteConditionExpression::IsBoardDependent() const
{
  if (IsBoardExpression()) {
    return !GetAttribute("Bname").empty();
  }
  if (GetExpressionDomain() == CONDITION_EXPRESSION) {
    return RteItem::IsBoardDependent();
  }
  return false;
}


string RteConditionExpression::ConstructID()
{
  auto it = m_attributes.begin();
  if (it != m_attributes.end()) {
    m_domain = it->first.at(0);
    if (m_domain == 'P') {
      m_domain = DEVICE_EXPRESSION;
    }
  }

  string id = GetTag() + " ";
  if (IsDependencyExpression()) {
    for (int i = 0; it != m_attributes.end(); it++, i++) {
      if (i)
        id += ":";
      id += it->second;
    }
  } else {
    id += GetAttributesString();
  }

  return id;
}


string RteConditionExpression::GetDisplayName() const
{
  return GetID();
}

const set<RteComponentAggregate*>& RteConditionExpression::GetComponentAggregates(RteTarget* target) const
{
  RteDependencySolver* solver = target->GetDependencySolver();
  return solver->GetComponentAggregates(const_cast<RteConditionExpression*>(this));
}

RteComponentAggregate* RteConditionExpression::GetSingleComponentAggregate(RteTarget* target) const
{
  return GetSingleComponentAggregate(target, GetComponentAggregates(target));
}

RteComponentAggregate* RteConditionExpression::GetSingleComponentAggregate(RteTarget* target, const set<RteComponentAggregate*>& components)
{
  if (components.empty()) {
    return nullptr;
  }
  RtePackage* devicePack = target->GetEffectiveDevicePackage();

  RteComponentAggregate* singleAggregate = nullptr;
  RtePackage* singleAggregatePack = nullptr;
  int nAggregates = 0;
  for (auto it = components.begin(); it != components.end(); it++) {
    RteComponentAggregate* a = *it;
    if (a->IsFiltered()) {
      RtePackage* aPack = a->GetPackage();
      if (!singleAggregate) {
        singleAggregate = a;
        singleAggregatePack = aPack;
        nAggregates = 1;
      } else if (a != singleAggregate) {
        if (aPack == devicePack) {
          if (singleAggregatePack == devicePack) {
            // more than one aggregate comes from device package
            return nullptr;
          }
          // aggregate comes from the same pack as device => it has preference
          singleAggregate = a;
          singleAggregatePack = aPack;
          nAggregates = 1;
        } else if (singleAggregatePack != devicePack) {
          nAggregates++;
        }
      }
    }
  }
  return nAggregates == 1 ? singleAggregate : nullptr;
}


bool RteConditionExpression::Validate()
{
  m_bValid = RteItem::Validate();

  map<string, string>::const_iterator it;
  bool bD = false;
  bool bB = false;
  bool bC = false;
  bool bT = false;
  bool bCondition = false;
  bool bMixed = false;
  for (it = m_attributes.begin(); it != m_attributes.end(); it++) {
    string a = it->first;
    char ch = a.at(0);
    switch (ch) {
    case 'C':
      if (!bC) {
        bC = true;
        bMixed = bD || bB || bT || bCondition;
        if (GetAttribute("Cclass").empty() || GetAttribute("Cgroup").empty()) {
          string msg = " '";
          msg += GetDisplayName() + "': Cclass or Cgroup attribute is missing in expression";
          m_errors.push_back(msg);
          m_bValid = false;
        }
      }
      break;
    case 'T':
      if (!bT) {
        bT = true;
        bMixed = bC || bD || bB || bCondition;
      }
      break;
    case 'c':
      bCondition = true;
      bMixed = bC || bD || bB || bT;
      break;
    case 'P':
    case 'D':
      if (!bD) {
        bD = true;
        bMixed = bC || bT || bB || bCondition;
      }
      break;
    case 'B':
      if (!bB) {
        bB = true;
        bMixed = bC || bT || bD || bCondition;
      }
      break;

    default:
      break;
    }; //case
  } // for

  if (bMixed) {
    string msg = " '";
    msg += GetDisplayName() + "': mixed 'B', 'C', 'D', 'T' or 'condition' attributes";
    m_errors.push_back(msg);
    m_bValid = false;
  }
  return m_bValid;
}


RteItem::ConditionResult RteConditionExpression::Evaluate(RteConditionContext* context)
{
  return context->EvaluateExpression(this);
}

RteItem::ConditionResult RteConditionExpression::GetConditionResult(RteConditionContext* context) const
{
  return context->GetConditionResult(const_cast<RteConditionExpression*>(this));
}

RteItem::ConditionResult RteConditionExpression::EvaluateExpression(RteTarget* target)
{
  if (!target)
    return FAILED;
  map<string, string>::const_iterator it, ita;
  const map<string, string>& attributes = target->GetAttributes();
  for (it = m_attributes.begin(); it != m_attributes.end(); it++) {
    const string& a = it->first;
    const string& v = it->second;
    if (a.empty())
      continue;
    if (a.at(0) == 'C') {
      continue; // skip check for Cclass, Cgroup, Csub, ...
    }
    if (a == "condition") {
      continue; // special handling for referred condition
    }
    ita = attributes.find(a);
    if (ita != attributes.end()) {
      const string& va = ita->second;
      if (a == "Dvendor" || a == "vendor") {
        if (!DeviceVendor::Match(va, v))
          return FAILED;
        continue;
      }
      if (a == "Dcdecp") {
        unsigned long uva = RteUtils::ToUL(va);
        unsigned long uv = RteUtils::ToUL(v);
        if ((uva & uv) == 0) // alternatively we have considered if ((uva & uv) == uv)
          return FAILED;
        continue;
      }
      // all other attributes
      if (!WildCards::Match(va, v))
        return FAILED;
    } else if (GetExpressionType() == DENY) {
      return FAILED; // for denied attributes, all must be given
    }
  }
  return FULFILLED;
}


RteItem::ConditionResult RteConditionExpression::GetDepsResult(map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const
{
  ConditionResult r = RteDependencyResult::GetResult(this, results);
  if (r != UNDEFINED)
    return r;
  RteItem::ConditionResult result = GetConditionResult(target->GetDependencySolver());
  if (result < FULFILLED && result > FAILED) {
    RteCondition* cond = GetCondition();
    if (cond) {
      return cond->GetDepsResult(results, target);
    } else if (IsDependencyExpression()) {
      // check if we already have result
      if (HasDepsResult(results))
        return result;
      RteDependencyResult depRes(this, result);
      const set<RteComponentAggregate*>& components = GetComponentAggregates(target);
      for (set<RteComponentAggregate*>::const_iterator it = components.begin(); it != components.end(); it++) {
        RteComponentAggregate* a = *it;
        if (a)
          depRes.AddComponentAggregate(a);
      }
      results[this] = depRes;
    }
  }
  return result;
}


bool RteConditionExpression::HasDepsResult(map<const RteItem*, RteDependencyResult>& results) const
{
  if (results.empty())
    return false;
  if (results.find(this) != results.end())
    return true;

  for (auto it = results.begin(); it != results.end(); it++) {
    const RteItem* item = it->first;
    if (item->GetTag() == GetTag() && item->EqualAttributes(this))
      return true;
  }

  return false;
}


RteItem::ConditionResult RteDenyExpression::Evaluate(RteConditionContext* context)
{
  ConditionResult result = RteConditionExpression::Evaluate(context);
  if (IsDependencyExpression()) {
    return result;
  } else if (result == R_ERROR) {
    return result;
  } else if (result == IGNORED) {
    return result;
  } else if (result == FAILED) {
    result = FULFILLED;
  } else {
    result = FAILED;
  }
  return result;
}


RteCondition::RteCondition(RteItem* parent) :
  RteItem(parent),
  m_bDeviceDependent(-1),
  m_bBoardDependent(-1),
  m_bInCheck(false)
{
}

RteCondition::~RteCondition()
{
  RteCondition::Clear();
}


const string& RteCondition::GetName() const
{
  return GetAttribute("id");
}

string RteCondition::GetDisplayName() const
{
  string s = GetTag() + " '" + GetName() + "'";
  return s;
}

bool RteCondition::Validate()
{
  m_bValid = RteItem::Validate();
  if (!m_bValid) {
    string msg = CreateErrorString("error", "502", "error(s) in condition definition:");
    m_errors.push_front(msg);
  }
  m_bInCheck = false;
  if (!ValidateRecursion()) {
    string msg = CreateErrorString("error", "503", "direct or indirect recursion detected");
    m_errors.push_back(msg);
    m_bValid = false;
  }
  return m_bValid;
}

bool RteCondition::ValidateRecursion()
{
  if (m_bInCheck) {
    return false;
  }
  m_bInCheck = true;
  bool noRecursion = true;
  list<RteItem*>::const_iterator it;
  for (it = m_children.begin(); it != m_children.end(); it++) {
    RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(*it);
    if (!expr)
      continue;

    RteCondition* cond = expr->GetCondition();
    if (!cond)
      continue;

    if (!cond->ValidateRecursion()) { // will return false if m_bInCheck == true
      noRecursion = false;
      break;
    }
  }
  m_bInCheck = false;
  return noRecursion;
}

void RteCondition::CalcDeviceAndBoardDependentFlags()
{
  if (m_bDeviceDependent < 0 || m_bBoardDependent < 0) { // not yet calculated
    m_bDeviceDependent = 0;
    m_bBoardDependent = 0;
    if (m_bInCheck) { // to prevent recursion
      return;
    }
    m_bInCheck = true;
    list<RteItem*>::const_iterator it;
    for (it = m_children.begin(); it != m_children.end(); it++) {
      RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(*it);
      if (!expr) {
        continue;
      }
      if (expr->IsDeviceDependent()) {
        m_bDeviceDependent = 1;
      }
      if (expr->IsBoardDependent()) {
        m_bBoardDependent = 1;
      }
      if (m_bDeviceDependent > 0 && m_bBoardDependent > 0) {
        break;
      }
    }
    m_bInCheck = false;
  }
}

bool RteCondition::IsDeviceDependent() const
{
  if (m_bDeviceDependent < 0) {
    RteCondition* that = const_cast<RteCondition*>(this);
    that->CalcDeviceAndBoardDependentFlags();
  }
  return m_bDeviceDependent > 0;
}

bool RteCondition::IsBoardDependent() const
{
  if (m_bBoardDependent < 0) {
    RteCondition* that = const_cast<RteCondition*>(this);
    that->CalcDeviceAndBoardDependentFlags();
  }
  return m_bBoardDependent > 0;
}


RteItem::ConditionResult RteCondition::GetDepsResult(map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const
{
  RteDependencySolver* solver = target->GetDependencySolver();
  RteItem::ConditionResult conditionResult = GetConditionResult(solver);
  ConditionResult resultAccept = FAILED;
  if (conditionResult < FULFILLED && conditionResult > FAILED) {
    bool hasAcceptConditions = false;
    list<RteItem*>::const_iterator it;
    // first collect all results from require and deny expressions
    for (it = m_children.begin(); it != m_children.end(); it++) {
      RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(*it);
      if (!expr)
        continue;
      if (expr->GetExpressionType() == RteConditionExpression::ACCEPT) {
        hasAcceptConditions = true;
        ConditionResult res = solver->GetConditionResult(expr);
        if (res > resultAccept) {
          resultAccept = res;
        }
        continue;
      }
      expr->GetDepsResult(results, target);
    }

    if (hasAcceptConditions) {
      // now collect results of accept expresssions
      // select only those with the results equal to acceptResult or the condition result
      for (it = m_children.begin(); it != m_children.end(); it++) {
        RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(*it);
        if (!expr || expr->GetExpressionType() != RteConditionExpression::ACCEPT)
          continue;
        ConditionResult res = solver->GetConditionResult(expr);
        if (res == resultAccept || res == conditionResult) {
          expr->GetDepsResult(results, target);
        }
      }
    }
  }
  return conditionResult;
}


RteItem::ConditionResult RteCondition::Evaluate(RteConditionContext* context)
{
  if (IsEvaluating(context)) {
    return R_ERROR; // recursion error
  }
  SetEvaluating(context, true);
  RteItem::ConditionResult result = EvaluateCondition(context);
  SetEvaluating(context, false);
  return result;
}

bool RteCondition::IsEvaluating(RteConditionContext* context) const
{
  return m_evaluating.find(context) != m_evaluating.end();
}

void RteCondition::SetEvaluating(RteConditionContext* context, bool evaluating)
{
  if (evaluating) {
    m_evaluating.insert(context);
  } else {
    m_evaluating.erase(context);
  }
}


RteItem::ConditionResult RteCondition::GetConditionResult(RteConditionContext* context) const
{
  return context->GetConditionResult(const_cast<RteCondition*>(this));
}


RteItem::ConditionResult RteCondition::EvaluateCondition(RteConditionContext* context) {
  return context->EvaluateCondition(this);
}


RteCondition* RteCondition::GetCondition() const
{
  return const_cast<RteCondition*>(this);
}

RteCondition* RteCondition::GetCondition(const string& id) const
{
  if (id == GetName())
    return GetCondition();
  return RteItem::GetCondition(id);
}

bool RteCondition::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  RteConditionExpression* expression = nullptr;
  const string& tag = xmlElement->GetTag();
  if (tag == "accept") {
    expression = new RteAcceptExpression(this);
  } else if (tag == "require") {
    expression = new RteRequireExpression(this);
  } else if (tag == "deny") {
    expression = new RteDenyExpression(this);
  }

  if (expression)
  {
    if (expression->Construct(xmlElement)) {
      AddItem(expression);
      return true;
    }
    delete expression;
    return false;
  }

  return RteItem::ProcessXmlElement(xmlElement);
}


RteConditionContainer::RteConditionContainer(RteItem* parent) :
  RteItem(parent)
{
}

bool RteConditionContainer::ProcessXmlElement(XMLTreeElement* xmlElement)
{
  const string& tag = xmlElement->GetTag();
  if (tag == "condition") {
    RteCondition* condition = new RteCondition(this);
    if (condition->Construct(xmlElement)) {
      AddItem(condition);
      return true;
    }
    delete condition;
    return false;
  }
  return RteItem::ProcessXmlElement(xmlElement);
}

RteDependencyResult::RteDependencyResult(const RteItem* item, RteItem::ConditionResult result) :
  m_item(item),
  m_result(result)
{
};
RteDependencyResult::~RteDependencyResult()
{
  Clear();
};


void RteDependencyResult::Clear()
{
  m_result = RteItem::UNDEFINED;
  m_aggregates.clear();
  m_results.clear();
}


string RteDependencyResult::GetDisplayName() const
{
  string name;
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  const RteComponentInstance* ci = dynamic_cast<const RteComponentInstance*>(m_item);
  const RteComponentAggregate* a = dynamic_cast<const RteComponentAggregate*>(m_item);
  if (c) {
    name = c->GetAggregateDisplayName();
  } else if (a) {
    name = a->GetFullDisplayName();
  } else if (ci) {
    name = ci->GetFullDisplayName();
  } else if (m_item && m_item == m_item->GetModel()) { // [TdB: 07.07.2015] m_item can be nullptr pointer
    name = "Validate Run Time Environment";
  } else if (m_item) {
    name = m_item->GetDisplayName();
  }
  return name;
}


string RteDependencyResult::GetMessageText() const
{
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  string message;
  // component or api
  if (c)
  {
    // check is we have several types of errors/warnings
    bool unresolved = false;
    bool conflicted = false;
    for (auto it = m_results.begin(); it != m_results.end(); it++) {
      const RteDependencyResult& depRes = it->second;
      RteItem::ConditionResult res = depRes.GetResult();
      switch (res) {
      case RteItem::INSTALLED:
      case RteItem::SELECTABLE:
      case RteItem::UNAVAILABLE:
      case RteItem::UNAVAILABLE_PACK:
      case RteItem::MISSING:
        unresolved = true;
        break;

      case RteItem::CONFLICT:
      case RteItem::INCOMPATIBLE:
      case RteItem::INCOMPATIBLE_VERSION:
      case RteItem::INCOMPATIBLE_VARIANT:
        conflicted = true;
        break;
      default:
        break;
      }
    }

    if (unresolved && conflicted) {
      message = "Component conflicts with other selected components and has unresolved dependencies";
      return message;
    }

    switch (m_result) {
    case RteItem::CONFLICT:
      message = "Conflict, select exactly one component from list:";
      break;
    case RteItem::INSTALLED:
    case RteItem::SELECTABLE:
    case RteItem::MISSING:
    case RteItem::UNAVAILABLE:
    case RteItem::UNAVAILABLE_PACK:
      message = "Additional software components required";
      break;
    case RteItem::INCOMPATIBLE:
      message = "Component is incompatible with other selected components";
      break;
    case RteItem::INCOMPATIBLE_VERSION:
      message = "Component is incompatible with versions of other selected components";
      break;
    case RteItem::INCOMPATIBLE_VARIANT:
      message = "Incompatible variant is selected";
      break;

    default:
      break;
    };
  } else if (m_item && m_item->GetModel() == m_item) {
    message = "Errors/Warnings detected:";
  } else {
    switch (m_result) {
    case RteItem::INSTALLED:
      message = "Select bundle and component from list";
      break;
    case RteItem::SELECTABLE:
      message = "Select component from list";
      break;
    case RteItem::MISSING:
      message = "Install missing component";
      break;
    case RteItem::UNAVAILABLE:
      message = "Install missing component or change target settings";
      break;
    case RteItem::UNAVAILABLE_PACK:
      message = "Update pack selection";
      break;
    case RteItem::MISSING_API:
      message = "API is missing";
      break;
    case RteItem::CONFLICT:
      message = "Conflict, select exactly one component from list";
      break;
    case RteItem::INCOMPATIBLE:
      message = "Select compatible component or unselect incompatible one";
      break;
    case RteItem::INCOMPATIBLE_VERSION:
      message = "Select compatible component version";
      break;
    case RteItem::INCOMPATIBLE_VARIANT:
      message = "Select compatible component variant";
      break;
    default:
      break;
    }
  }
  return message;
}


string RteDependencyResult::GetErrorNum() const
{
  string errNum;
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  if (c) {
    switch (m_result) {
    case RteItem::INSTALLED:
      errNum = "510";
      break;
    case RteItem::SELECTABLE:
      errNum = "515";
      break;
    case RteItem::MISSING:
      errNum = "511";
      break;
    case RteItem::CONFLICT:
      errNum = "512";
      break;
    case RteItem::INCOMPATIBLE:
      errNum = "513";
      break;
    case RteItem::INCOMPATIBLE_VERSION:
      errNum = "514";
      break;
    case RteItem::INCOMPATIBLE_VARIANT:
      errNum = "516";
      break;
    default:
      break;
    };
  }
  return errNum;
}

string RteDependencyResult::GetSeverity() const
{
  string severity;
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  if (c) {
    if (m_result == RteItem::INSTALLED || m_result == RteItem::SELECTABLE)
      severity = "warning";
    else
      severity = "error";
  }
  return severity;
}


string RteDependencyResult::GetOutputMessage() const
{
  string output;
  const RteComponent* c = dynamic_cast<const RteComponent*>(m_item);
  if (c)
  {
    output = "'";
    output += c->GetFullDisplayName();

    output += "': " + GetSeverity() + " #" + GetErrorNum() + ": " + GetMessageText();
  } else if (m_item && m_item->GetModel() == m_item) {
    output = "Validate Run Time Environment: errors/warnings detected:";
    return output;
  } else if (m_item) {
    output = " ";
    output += m_item->GetDisplayName();
    output += ": " + GetMessageText();
  }
  return output;
}


void RteDependencyResult::AddComponentAggregate(RteComponentAggregate* a)
{
  m_aggregates.insert(a);
}

RteItem::ConditionResult RteDependencyResult::GetResult(const RteItem* item, const map<const RteItem*, RteDependencyResult>& results)
{
  auto it = results.find(item);
  if (it != results.end())
    return it->second.GetResult();

  return RteItem::UNDEFINED;
}


RteConditionContext::RteConditionContext(RteTarget* target) :
  m_target(target),
  m_result(RteItem::UNDEFINED)
{
}

RteConditionContext::~RteConditionContext()
{
  RteConditionContext::Clear();
}

void RteConditionContext::Clear()
{
  m_result = RteItem::IGNORED;
  m_cachedResults.clear();
}

RteItem::ConditionResult RteConditionContext::Evaluate(RteItem* item)
{
  RteItem::ConditionResult res = GetConditionResult(item);
  if (res == RteItem::UNDEFINED) {
    res = item->Evaluate(this);
    m_cachedResults[item] = res;
  }
  return res;

}

RteItem::ConditionResult RteConditionContext::GetConditionResult(RteItem* item) const
{
  if (!item)
    return RteItem::R_ERROR;
  auto it = m_cachedResults.find(item);
  if (it != m_cachedResults.end())
    return it->second;
  return RteItem::UNDEFINED;
}

RteItem::ConditionResult RteConditionContext::EvaluateCondition(RteCondition* condition)
{
  RteItem::ConditionResult resultRequire = RteItem::IGNORED;
  RteItem::ConditionResult resultAccept = RteItem::UNDEFINED;
  // first check require and deny expressions
  const list<RteItem*>& children = condition->GetChildren();
  for (auto it = children.begin(); it != children.end(); it++) {
    RteConditionExpression* expr = dynamic_cast<RteConditionExpression*>(*it);
    if (!expr)
      continue;
    RteItem::ConditionResult res = Evaluate(expr);
    if (res == RteItem::R_ERROR)
      return res;
    if (res == RteItem::IGNORED || res == RteItem::UNDEFINED)
      continue;

    if (expr->GetExpressionType() == RteConditionExpression::ACCEPT) {
      if (res > resultAccept) {
        resultAccept = res;
      }
    } else { // deny or require
      if (res < resultRequire) {
        resultRequire = res;
      }
    }
  }

  if (resultAccept != RteItem::UNDEFINED && resultAccept < resultRequire) {
    return resultAccept;
  }
  return resultRequire;
}

RteItem::ConditionResult RteConditionContext::EvaluateExpression(RteConditionExpression* expr)
{
  if (!expr)
    return RteItem::R_ERROR;
  char domain = expr->GetExpressionDomain();
  switch (domain)
  {
  case BOARD_EXPRESSION:
  case DEVICE_EXPRESSION:
  case TOOLCHAIN_EXPRESSION:
    return expr->EvaluateExpression(GetTarget());

  case COMPONENT_EXPRESSION:
    return RteItem::IGNORED;

  case CONDITION_EXPRESSION:
    return Evaluate(expr->GetCondition());

  default:
    return RteItem::R_ERROR;
  };
}


RteDependencySolver::RteDependencySolver(RteTarget* target) :
  RteConditionContext(target)
{
}

RteDependencySolver::~RteDependencySolver()
{
  RteDependencySolver::Clear();
}


void RteDependencySolver::Clear()
{
  RteConditionContext::Clear();
  m_componentAggregates.clear();
}

RteItem::ConditionResult RteDependencySolver::EvaluateCondition(RteCondition* condition)
{
  // new behaviour - first check if filtering condition evaluates to FULFILLED or IGNORED
  RteConditionContext* filterContext = GetTarget()->GetFilterContext();
  RteItem::ConditionResult res = filterContext->Evaluate(condition);
  switch (res) {
  case RteItem::FAILED: //  ignore dependencies if filter context failed
    return RteItem::IGNORED;
  case RteItem::R_ERROR:
    return RteItem::R_ERROR;
  default:
    break;
  }
  return RteConditionContext::EvaluateCondition(condition);
}

RteItem::ConditionResult RteDependencySolver::EvaluateExpression(RteConditionExpression* expr)
{
  if (!expr)
    return RteItem::R_ERROR;
  char domain = expr->GetExpressionDomain();
  switch (domain)
  {
  case BOARD_EXPRESSION:
  case DEVICE_EXPRESSION:
  case TOOLCHAIN_EXPRESSION:
    return RteItem::IGNORED;

  case COMPONENT_EXPRESSION:
    return CalculateDependencies(expr);

  case CONDITION_EXPRESSION:
    return Evaluate(expr->GetCondition());

  default:
    return RteItem::R_ERROR;
  };
}

RteItem::ConditionResult RteDependencySolver::CalculateDependencies(RteConditionExpression* expr)
{
  set<RteComponentAggregate*> components;
  RteItem::ConditionResult result;
  if (expr->IsDenyExpression()) {
    result = RteItem::FULFILLED;
    const map<RteComponentAggregate*, int>& selectedComponents = m_target->GetSelectedComponentAggregates();
    for (auto it = selectedComponents.begin(); it != selectedComponents.end(); it++) {
      RteComponentAggregate* a = it->first;
      RteItem* c = a->GetComponent();
      if (!c)
        c = a->GetComponentInstance();
      if (c && c->HasComponentAttributes(expr->GetAttributes())) {
        components.insert(a);
        result = RteItem::INCOMPATIBLE;
      }
    }
  } else {
    result = m_target->GetComponentAggregates(*expr, components);
    if (components.size() > 1) {
      // leave only the component if it can be relsolved automatically (current bundle, DFP)
      RteComponentAggregate* a = expr->GetSingleComponentAggregate(m_target, components);
      if (a) {
        components.clear();
        components.insert(a);
      }
    }
  }
  m_componentAggregates[expr] = components;
  return result;
}

const set<RteComponentAggregate*>& RteDependencySolver::GetComponentAggregates(RteConditionExpression* expr) const
{
  auto it = m_componentAggregates.find(expr);
  if (it != m_componentAggregates.end())
    return it->second;

  static set<RteComponentAggregate*> emptyComponentSet;
  return emptyComponentSet;
}


RteItem::ConditionResult RteDependencySolver::EvaluateDependencies()
{
  Clear();
  const map<RteComponentAggregate*, int>& selectedComponents = m_target->GetSelectedComponentAggregates();
  for (auto it = selectedComponents.begin(); it != selectedComponents.end(); it++) {
    RteComponentAggregate* a = it->first;
    RteItem::ConditionResult res = a->Evaluate(this);
    if (res > RteItem::UNDEFINED && m_result > res)
      m_result = res;
  }
  return GetConditionResult();
}

RteItem::ConditionResult RteDependencySolver::ResolveDependencies()
{
  for (RteItem::ConditionResult res = GetConditionResult(); res < RteItem::FULFILLED; res = GetConditionResult()) {
    if (ResolveIteration() == false)
      break;
  }
  return GetConditionResult();
}

/**
* Tries to resolve SELECTABLE dependencies
* @return true if one of dependencies gets resolved => the state changes
*/
bool RteDependencySolver::ResolveIteration()
{
  if (!m_target || !m_target->GetClasses())
    return false;
  map<const RteItem*, RteDependencyResult> results;
  m_target->GetSelectedDepsResult(results, m_target);

  for (auto it = results.begin(); it != results.end(); it++) {
    const RteDependencyResult& depsRes = it->second;
    RteItem::ConditionResult r = depsRes.GetResult();
    if (r != RteItem::SELECTABLE)
      continue;
    if (ResolveDependency(depsRes))
      return true;
  }
  return false;
}


bool RteDependencySolver::ResolveDependency(const RteDependencyResult& depsRes)
{
  // add subitems if any
  const map<const RteItem*, RteDependencyResult>& results = depsRes.GetResults();
  map<const RteItem*, RteDependencyResult>::const_iterator it;
  for (it = results.begin(); it != results.end(); it++) {
    const RteDependencyResult& dRes = it->second;
    RteItem::ConditionResult r = dRes.GetResult();
    if (r != RteItem::SELECTABLE) {
      continue;
    }
    const RteItem* item = dRes.GetItem();

    const RteConditionExpression* expr = dynamic_cast<const RteConditionExpression*>(item);
    RteComponentAggregate* a = expr->GetSingleComponentAggregate(m_target);
    if (a) {
      RteComponent* c = a->GetComponent();
      if (!c) {
        continue;
      }
      if (!c->HasComponentAttributes(expr->GetAttributes())) {
        c = a->FindComponent(expr->GetAttributes());
        if (c) {
          a->SetSelectedVariant(c->GetCvariantName());
          a->SetSelectedVersion(c->GetVersionString());
        }
      }
      if (c->IsCustom()) {
        // Disable "Resolve" function for components with 'custom=1' attribute
        continue;
      }

      m_target->SelectComponent(a, 1, true); // will trigger EvaluateDependencies()
      return true;
    }
  }
  return false;
}

// End of RteCondition.cpp
