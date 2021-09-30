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

#pragma once

#include <libyul/optimiser/ASTWalker.h>

#include <set>
#include <stack>
#include <optional>

namespace solidity::yul
{

struct Dialect;

/// Whether an effect, depending on the input, might happen, always happens
/// or never happens.
enum Modality
{
	Never,
	Maybe,
	Always
};

}
/// Serial (consecutive) combination of two effects.
inline solidity::yul::Modality operator+=(solidity::yul::Modality _a, solidity::yul::Modality _b)
{
	return static_cast<solidity::yul::Modality>(std::max<int>(static_cast<int>(_a), static_cast<int>(_b)));
}

/// Parallel (alternative) combination of two effects.
inline solidity::yul::Modality operator|=(solidity::yul::Modality _a, solidity::yul::Modality _b)
{
	return _a == _b ? _a : solidity::yul::Modality::Maybe;
}

namespace solidity::yul
{

/**
 * Side effects of code related to control flow.
 */
struct ControlFlowSideEffects
{
//	/// If true, this code terminates the control flow.
//	/// State may or may not be reverted as indicated by the ``reverts`` flag.
//	bool terminates = false;
//	/// If true, this code reverts all state changes in the transaction.
//	/// Whenever this is true, ``terminates`` has to be true as well.
//	bool reverts = false;

	// TODO handle "loops"
	Modality loops = Modality::Never;
	Modality terminates = Modality::Never;
	Modality reverts = Modality::Never;

	/// Serial (consecutive) combination of two side effects.
	ControlFlowSideEffects& operator+=(ControlFlowSideEffects const& _other);
	/// Parallel (alternative) combination of two side effects.
	ControlFlowSideEffects& operator|=(ControlFlowSideEffects const& _other);
};

/**
 * Requires: Disambiguator.
 */
class ControlFlowSideEffectsCollector: public ASTWalker
{
public:
	static std::map<YulString, ControlFlowSideEffects> sideEffectsOfFunctions(
		Block const& _ast,
		Dialect const& _dialect
	);
//	static ControlFlowSideEffects sideEffectsOfCode(
//		Expression const& _ast,
//		Dialect const& _dialect
//	);
//	ControlFlowSideEffectsCollector(
//		Dialect const& _dialect,
//		std::map<YulString, ControlFlowSideEffects> _functionSideEffects
//	);

	using ASTWalker::operator();
	void operator()(FunctionCall const& _functionCall) override;
	void operator()(If const& _if) override;
	void operator()(Switch const& _switch) override;
	void operator()(FunctionDefinition const& _functionDefinition) override;
	void operator()(ForLoop const& _forLoop) override;
	void operator()(Break const& _break) override;
	void operator()(Continue const& _continue) override;
	void operator()(Leave const& _leave) override;

private:
	explicit ControlFlowSideEffectsCollector(Dialect const& _dialect):
		m_dialect(_dialect)
	{}

	Dialect const& m_dialect;
	std::map<YulString, FunctionDefinition const*> m_functions;
	std::map<YulString, ControlFlowSideEffects> m_functionSideEffects;
	ControlFlowSideEffects m_sideEffects;
	struct
	{
		std::optional<ControlFlowSideEffects> pendingBreak;
		std::optional<ControlFlowSideEffects> pendingContinue;
		std::optional<ControlFlowSideEffects> pendingLeave;
	} m_contextInfo;
};


}
