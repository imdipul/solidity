/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

#include <libyul/ControlFlowSideEffectsCollector.h>

#include <libyul/optimiser/FunctionDefinitionCollector.h>

#include <libyul/AST.h>
#include <libyul/Dialect.h>

#include <libsolutil/Common.h>

#include <range/v3/view/reverse.hpp>

using namespace std;
using namespace solidity::yul;

/// Serial (consecutive) combination of two effects.
inline solidity::yul::Modality& operator+=(solidity::yul::Modality& _a, solidity::yul::Modality _b)
{
	_a = static_cast<solidity::yul::Modality>(std::max<int>(static_cast<int>(_a), static_cast<int>(_b)));
	return _a;
}

/// Parallel (alternative) combination of two effects.
inline solidity::yul::Modality& operator|=(solidity::yul::Modality& _a, solidity::yul::Modality _b)
{
	if (_a != _b)
		_a = solidity::yul::Modality::Maybe;
	return _a;
}


inline ControlFlowSideEffects& operator+=(ControlFlowSideEffects& _me, ControlFlowSideEffects const& _other)
{
	_me.loops += _other.loops;
	_me.terminates += _other.terminates;
	_me.reverts += _other.reverts;
	return _me;
}

inline ControlFlowSideEffects& operator|=(ControlFlowSideEffects& _me, ControlFlowSideEffects const& _other)
{
	_me.loops |= _other.loops;
	_me.terminates |= _other.terminates;
	_me.reverts |= _other.reverts;
	return _me;
}

namespace
{
// TODO why does this not work?
//std::optional<ControlFlowSideEffects>& operator|=(
//	std::optional<ControlFlowSideEffects>& _a,
//	ControlFlowSideEffects const& _b
//)
//{
//	if (_a)
//		*_a |= _b;
//	else
//		_a = {_b};

//	return _a;
//}
void combine(
	optional<ControlFlowSideEffects>& _a,
	ControlFlowSideEffects const& _b
)
{
	if (_a)
		*_a |= _b;
	else
		_a = {_b};
}
}

map<YulString, ControlFlowSideEffects>
ControlFlowSideEffectsCollector::sideEffectsOfFunctions(Block const& _ast, Dialect const& _dialect)
{
	ControlFlowSideEffectsCollector collector(_dialect);
	collector.m_functions = FunctionDefinitionCollector::run(_ast);
	collector(_ast);
	return collector.m_functionSideEffects;
}

void ControlFlowSideEffectsCollector::operator()(FunctionCall const& _functionCall)
{
	walkVector(_functionCall.arguments | ranges::views::reverse);

	if (auto const* builtin = m_dialect.builtin(_functionCall.functionName.name))
		m_sideEffects += builtin->controlFlowSideEffects;
	else
	{
		// TODO handle recursion
		if (!m_functionSideEffects.count(_functionCall.functionName.name))
			(*this)(*m_functions.at(_functionCall.functionName.name));
		m_sideEffects += m_functionSideEffects.at(_functionCall.functionName.name);
	}
}

void ControlFlowSideEffectsCollector::operator()(If const& _if)
{
	visit(*_if.condition);
	ControlFlowSideEffects sideEffects = m_sideEffects;
	(*this)(_if.body);
	m_sideEffects |= sideEffects;
}

void ControlFlowSideEffectsCollector::operator()(Switch const& _switch)
{
	visit(*_switch.expression);
	ControlFlowSideEffects initialSideEffects = m_sideEffects;

	optional<ControlFlowSideEffects> finalSideEffects;

	if (_switch.cases.back().value)
		// no default, add "jump over"
		finalSideEffects = initialSideEffects;

	for (Case const& case_: _switch.cases)
	{
		m_sideEffects = initialSideEffects;
		(*this)(case_.body);
		if (finalSideEffects)
			*finalSideEffects |= m_sideEffects;
		else
			finalSideEffects = m_sideEffects;
	}
	m_sideEffects = *finalSideEffects;
}

void ControlFlowSideEffectsCollector::operator()(FunctionDefinition const& _funDef)
{
	if (m_functionSideEffects.count(_funDef.name))
		return;

	ScopedSaveAndRestore sideEffects(m_sideEffects, {});
	ScopedSaveAndRestore contextInfo(m_contextInfo, {});

	ASTWalker::operator()(_funDef);

	if (m_contextInfo.pendingLeave)
		m_sideEffects |= *m_contextInfo.pendingLeave;

	m_functionSideEffects[_funDef.name] = m_sideEffects;
}

void ControlFlowSideEffectsCollector::operator()(ForLoop const& _for)
{
	ScopedSaveAndRestore pendingBreak(m_contextInfo.pendingBreak, {});
	ScopedSaveAndRestore pendingContinue(m_contextInfo.pendingContinue, {});

	(*this)(_for.pre);
	visit(*_for.condition);

	ControlFlowSideEffects sideEffects = m_sideEffects;

	(*this)(_for.body);
	if (m_contextInfo.pendingContinue)
		m_sideEffects |= *m_contextInfo.pendingContinue;
	(*this)(_for.post);
	// TODO do we have to run twice?

	if (m_contextInfo.pendingBreak)
		m_sideEffects |= *m_contextInfo.pendingBreak;

	m_sideEffects |= sideEffects;
}

void ControlFlowSideEffectsCollector::operator()(Break const&)
{
	combine(m_contextInfo.pendingBreak, m_sideEffects);
	// We cannot clear m_sideEffects because the breaking branch
	// still counts into the other branch because it branched
	// off from it.
}

void ControlFlowSideEffectsCollector::operator()(Continue const&)
{
	combine(m_contextInfo.pendingContinue, m_sideEffects);
}

void ControlFlowSideEffectsCollector::operator()(Leave const&)
{
	// With this, { leave revert(0, 0) } results in "maybe revert", and not "always revert".
	combine(m_contextInfo.pendingLeave, m_sideEffects);
}
