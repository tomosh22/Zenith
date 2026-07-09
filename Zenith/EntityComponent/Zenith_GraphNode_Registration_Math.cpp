#include "Zenith.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "EntityComponent/Zenith_GraphOps.h"

#include <cmath>

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - Blackboard/Maths domain.
//
// Pure blackboard-in / blackboard-out nodes (no engine systems). The scalar
// Set/Add/Compare family lives in the core registration TU; this TU holds the
// list utilities and the math family (MathBlackboardFloat/Vector3, Lerp*,
// Clamp, Random*, Add* siblings).
//
// Conventions: an optional m_str*Var property overrides its constant twin when
// set; m_strResultVar defaulting "" means "write back in place" for nodes
// whose natural output type matches the input.
//------------------------------------------------------------------------------

namespace
{
	// List length -> int32 var (0 when the list is absent).
	class Zenith_GraphNode_GetListCount : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_GetListCount)
	public:
		ZENITH_PROPERTY(std::string, m_strListVar, "list")
		ZENITH_PROPERTY(std::string, m_strResultVar, "count")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const Zenith_Vector<Zenith_PropertyValue>* pxList = xContext.m_pxBlackboard->TryGetList(m_strListVar);
			Zenith_PropertyValue xCount;
			xCount.SetInt32(pxList ? static_cast<int32_t>(pxList->GetSize()) : 0);
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xCount);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "GetListCount"; }
	};

	// One list element -> a plain blackboard var. Index from a const or an
	// int32 var (var wins when set). FAILURE when out of range / list absent -
	// doubles as the bounds gate.
	class Zenith_GraphNode_GetListElement : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_GetListElement)
	public:
		ZENITH_PROPERTY(std::string, m_strListVar, "list")
		ZENITH_PROPERTY(int32_t, m_iIndex, 0)
		ZENITH_PROPERTY(std::string, m_strIndexVar, "")
		ZENITH_PROPERTY(std::string, m_strResultVar, "item")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const Zenith_Vector<Zenith_PropertyValue>* pxList = xContext.m_pxBlackboard->TryGetList(m_strListVar);
			const int32_t iIndex = m_strIndexVar.empty()
				? m_iIndex : xContext.m_pxBlackboard->GetInt32(m_strIndexVar, m_iIndex);
			if (!pxList || iIndex < 0 || iIndex >= static_cast<int32_t>(pxList->GetSize()))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			xContext.m_pxBlackboard->SetValue(m_strResultVar, pxList->Get(static_cast<u_int>(iIndex)));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "GetListElement"; }
	};

	//==========================================================================
	// Scalar / vector arithmetic
	//==========================================================================

	// Integer sibling of AddBlackboardFloat: variable += delta (const or var).
	class Zenith_GraphNode_AddBlackboardInt : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_AddBlackboardInt)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "value")
		ZENITH_PROPERTY(int32_t, m_iDelta, 1)
		ZENITH_PROPERTY(std::string, m_strDeltaVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const int32_t iDelta = m_strDeltaVar.empty()
				? m_iDelta : xContext.m_pxBlackboard->GetInt32(m_strDeltaVar, m_iDelta);
			Zenith_PropertyValue xValue;
			xValue.SetInt32(xContext.m_pxBlackboard->GetInt32(m_strVariable, 0) + iDelta);
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "AddBlackboardInt"; }
	};

	// Vector sibling: variable += delta (const or var), optionally scaled by
	// the dispatching event's dt - the position/velocity integrate primitive.
	class Zenith_GraphNode_AddBlackboardVector3 : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_AddBlackboardVector3)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "vec")
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xDelta, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strDeltaVar, "")
		ZENITH_PROPERTY(bool, m_bScaleByDt, false)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector3 xDelta = m_strDeltaVar.empty()
				? m_xDelta : xContext.m_pxBlackboard->GetVector3(m_strDeltaVar, m_xDelta);
			if (m_bScaleByDt)
			{
				xDelta *= xContext.m_fDt;
			}
			Zenith_PropertyValue xValue;
			xValue.SetVector3(xContext.m_pxBlackboard->GetVector3(m_strVariable) + xDelta);
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "AddBlackboardVector3"; }
	};

	// Equality test for two packed-EntityID vars -> bool var (the BT
	// target-changed gate). Op: 0 = equal, 1 = notEqual. Missing vars compare
	// as 0 (the "no entity" value).
	class Zenith_GraphNode_CompareBlackboardEntity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_CompareBlackboardEntity)
	public:
		ZENITH_PROPERTY(std::string, m_strVarA, "a")
		ZENITH_PROPERTY(std::string, m_strVarB, "b")
		ZENITH_PROPERTY(int32_t, m_iOp, 0)
		ZENITH_PROPERTY(std::string, m_strResultVar, "result")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const u_int64 ulA = xContext.m_pxBlackboard->GetPackedEntityID(m_strVarA, 0);
			const u_int64 ulB = xContext.m_pxBlackboard->GetPackedEntityID(m_strVarB, 0);
			// Ternary, not a defaulting switch: any op other than NOT_EQUAL means
			// EQUAL (preserves the original out-of-range behaviour).
			Zenith_PropertyValue xResult;
			xResult.SetBool(static_cast<Zenith_GraphEntityCompareOp>(m_iOp) == GRAPH_ENTITY_COMPARE_OP_NOT_EQUAL
				? (ulA != ulB) : (ulA == ulB));
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "CompareBlackboardEntity"; }
	};

	// Float arithmetic on a blackboard var. m_iOp is a Zenith_GraphMathFloatOp
	// (ABS/SIN/COS are unary - operand ignored). Operand from const or var;
	// result to m_strResultVar ("" = in place).
	// Division/modulo by zero = FAILURE (fail loudly, not NaN-quietly).
	class Zenith_GraphNode_MathBlackboardFloat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_MathBlackboardFloat)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "value")
		ZENITH_PROPERTY(int32_t, m_iOp, 0)
		ZENITH_PROPERTY(float, m_fOperand, 1.0f)
		ZENITH_PROPERTY(std::string, m_strOperandVar, "")
		ZENITH_PROPERTY(std::string, m_strResultVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const float fValue = xContext.m_pxBlackboard->GetFloat(m_strVar);
			const float fOperand = m_strOperandVar.empty()
				? m_fOperand : xContext.m_pxBlackboard->GetFloat(m_strOperandVar, m_fOperand);
			float fResult = 0.0f;
			switch (static_cast<Zenith_GraphMathFloatOp>(m_iOp))
			{
			case GRAPH_MATH_FLOAT_OP_SUBTRACT: fResult = fValue - fOperand; break;
			case GRAPH_MATH_FLOAT_OP_MULTIPLY: fResult = fValue * fOperand; break;
			case GRAPH_MATH_FLOAT_OP_DIVIDE:
				if (fOperand == 0.0f) { return GRAPH_NODE_STATUS_FAILURE; }
				fResult = fValue / fOperand;
				break;
			case GRAPH_MATH_FLOAT_OP_MODULO:
				if (fOperand == 0.0f) { return GRAPH_NODE_STATUS_FAILURE; }
				fResult = std::fmod(fValue, fOperand);
				break;
			case GRAPH_MATH_FLOAT_OP_MIN: fResult = fValue < fOperand ? fValue : fOperand; break;
			case GRAPH_MATH_FLOAT_OP_MAX: fResult = fValue > fOperand ? fValue : fOperand; break;
			case GRAPH_MATH_FLOAT_OP_ABS: fResult = std::fabs(fValue); break;
			case GRAPH_MATH_FLOAT_OP_SIN: fResult = std::sin(fValue); break;
			case GRAPH_MATH_FLOAT_OP_COS: fResult = std::cos(fValue); break;
			default: return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xResult;
			xResult.SetFloat(fResult);
			xContext.m_pxBlackboard->SetValue(m_strResultVar.empty() ? m_strVar : m_strResultVar, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "MathBlackboardFloat"; }
	};

	// Vector3 arithmetic on a blackboard var. Op: 0 = add, 1 = sub (vec
	// operand), 2 = scale (scalar operand), 3 = normalize (unary), 4 = length
	// (unary -> FLOAT result), 5 = dot (vec operand -> FLOAT result),
	// 6 = clampLength (scalar operand = max magnitude). Vec operand from
	// m_strOperandVar; scalar from m_fScalar / m_strScalarVar. Result to
	// m_strResultVar ("" = in place; length/dot then still write in place,
	// replacing the vec with a float - point them at a result var).
	class Zenith_GraphNode_MathBlackboardVector3 : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_MathBlackboardVector3)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "vec")
		ZENITH_PROPERTY(int32_t, m_iOp, 0)
		ZENITH_PROPERTY(std::string, m_strOperandVar, "")
		ZENITH_PROPERTY(float, m_fScalar, 1.0f)
		ZENITH_PROPERTY(std::string, m_strScalarVar, "")
		ZENITH_PROPERTY(std::string, m_strResultVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const Zenith_Maths::Vector3 xValue = xContext.m_pxBlackboard->GetVector3(m_strVar);
			const Zenith_Maths::Vector3 xOperand = m_strOperandVar.empty()
				? Zenith_Maths::Vector3(0.0f) : xContext.m_pxBlackboard->GetVector3(m_strOperandVar);
			const float fScalar = m_strScalarVar.empty()
				? m_fScalar : xContext.m_pxBlackboard->GetFloat(m_strScalarVar, m_fScalar);
			const std::string& strOut = m_strResultVar.empty() ? m_strVar : m_strResultVar;

			Zenith_PropertyValue xResult;
			switch (m_iOp)
			{
			case 0: xResult.SetVector3(xValue + xOperand); break;
			case 1: xResult.SetVector3(xValue - xOperand); break;
			case 2: xResult.SetVector3(xValue * fScalar); break;
			case 3:
			{
				const float fLength = glm::length(xValue);
				xResult.SetVector3(fLength > 0.0001f ? xValue / fLength : Zenith_Maths::Vector3(0.0f));
				break;
			}
			case 4: xResult.SetFloat(glm::length(xValue)); break;
			case 5: xResult.SetFloat(glm::dot(xValue, xOperand)); break;
			case 6:
			{
				const float fLength = glm::length(xValue);
				xResult.SetVector3((fLength > fScalar && fLength > 0.0001f) ? xValue * (fScalar / fLength) : xValue);
				break;
			}
			default: return GRAPH_NODE_STATUS_FAILURE;
			}
			xContext.m_pxBlackboard->SetValue(strOut, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "MathBlackboardVector3"; }
	};

	// Shared easing for the Lerp nodes. 0 = linear, 1 = smoothstep,
	// 2 = easeInQuad, 3 = easeOutQuad.
	float ApplyEasing(int32_t iEasing, float fT)
	{
		fT = fT < 0.0f ? 0.0f : (fT > 1.0f ? 1.0f : fT);
		switch (iEasing)
		{
		case 1: return fT * fT * (3.0f - 2.0f * fT);
		case 2: return fT * fT;
		case 3: return fT * (2.0f - fT);
		default: return fT;
		}
	}

	// Blends a float var toward a target. Mode 0 (t-mode): value = lerp(value,
	// target, ease(t)) per execute - t from const or var. Mode 1 (rate-mode):
	// value moves toward target at m_fRate units/second of dispatched dt,
	// clamping at the target (easing ignored). Always SUCCESS; in-place.
	class Zenith_GraphNode_LerpBlackboardFloat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_LerpBlackboardFloat)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "value")
		ZENITH_PROPERTY(float, m_fTarget, 0.0f)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")
		ZENITH_PROPERTY(int32_t, m_iMode, 0)
		ZENITH_PROPERTY_RANGED(float, m_fT, 0.5f, 0.0f, 1.0f)
		ZENITH_PROPERTY(std::string, m_strTVar, "")
		ZENITH_PROPERTY_RANGED(float, m_fRate, 1.0f, 0.0f, 100000.0f)
		ZENITH_PROPERTY(int32_t, m_iEasing, 0)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const float fValue = xContext.m_pxBlackboard->GetFloat(m_strVar);
			const float fTarget = m_strTargetVar.empty()
				? m_fTarget : xContext.m_pxBlackboard->GetFloat(m_strTargetVar, m_fTarget);
			float fResult;
			if (m_iMode == 1)
			{
				const float fStep = m_fRate * xContext.m_fDt;
				const float fDelta = fTarget - fValue;
				fResult = std::fabs(fDelta) <= fStep ? fTarget : fValue + (fDelta > 0.0f ? fStep : -fStep);
			}
			else
			{
				const float fT = ApplyEasing(m_iEasing, m_strTVar.empty()
					? m_fT : xContext.m_pxBlackboard->GetFloat(m_strTVar, m_fT));
				fResult = fValue + (fTarget - fValue) * fT;
			}
			Zenith_PropertyValue xResult;
			xResult.SetFloat(fResult);
			xContext.m_pxBlackboard->SetValue(m_strVar, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "LerpBlackboardFloat"; }
	};

	// Vector3 twin of LerpBlackboardFloat (rate-mode moves along the delta
	// direction at m_fRate units/second, clamping at the target).
	class Zenith_GraphNode_LerpBlackboardVector3 : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_LerpBlackboardVector3)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "vec")
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xTarget, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")
		ZENITH_PROPERTY(int32_t, m_iMode, 0)
		ZENITH_PROPERTY_RANGED(float, m_fT, 0.5f, 0.0f, 1.0f)
		ZENITH_PROPERTY(std::string, m_strTVar, "")
		ZENITH_PROPERTY_RANGED(float, m_fRate, 1.0f, 0.0f, 100000.0f)
		ZENITH_PROPERTY(int32_t, m_iEasing, 0)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const Zenith_Maths::Vector3 xValue = xContext.m_pxBlackboard->GetVector3(m_strVar);
			const Zenith_Maths::Vector3 xTarget = m_strTargetVar.empty()
				? m_xTarget : xContext.m_pxBlackboard->GetVector3(m_strTargetVar, m_xTarget);
			Zenith_Maths::Vector3 xResult;
			if (m_iMode == 1)
			{
				const Zenith_Maths::Vector3 xDelta = xTarget - xValue;
				const float fDistance = glm::length(xDelta);
				const float fStep = m_fRate * xContext.m_fDt;
				xResult = (fDistance <= fStep || fDistance < 0.0001f)
					? xTarget : xValue + xDelta * (fStep / fDistance);
			}
			else
			{
				const float fT = ApplyEasing(m_iEasing, m_strTVar.empty()
					? m_fT : xContext.m_pxBlackboard->GetFloat(m_strTVar, m_fT));
				xResult = xValue + (xTarget - xValue) * fT;
			}
			Zenith_PropertyValue xValueOut;
			xValueOut.SetVector3(xResult);
			xContext.m_pxBlackboard->SetValue(m_strVar, xValueOut);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "LerpBlackboardVector3"; }
	};

	// Clamps a float var into [min, max] (consts or var overrides). In place.
	class Zenith_GraphNode_ClampBlackboardFloat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ClampBlackboardFloat)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "value")
		ZENITH_PROPERTY(float, m_fMin, 0.0f)
		ZENITH_PROPERTY(std::string, m_strMinVar, "")
		ZENITH_PROPERTY(float, m_fMax, 1.0f)
		ZENITH_PROPERTY(std::string, m_strMaxVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const float fMin = m_strMinVar.empty()
				? m_fMin : xContext.m_pxBlackboard->GetFloat(m_strMinVar, m_fMin);
			const float fMax = m_strMaxVar.empty()
				? m_fMax : xContext.m_pxBlackboard->GetFloat(m_strMaxVar, m_fMax);
			float fValue = xContext.m_pxBlackboard->GetFloat(m_strVar);
			fValue = fValue < fMin ? fMin : (fValue > fMax ? fMax : fValue);
			Zenith_PropertyValue xResult;
			xResult.SetFloat(fValue);
			xContext.m_pxBlackboard->SetValue(m_strVar, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ClampBlackboardFloat"; }
	};

	//==========================================================================
	// Random - per-instance xorshift64* PRNG. m_iSeed != 0 gives a
	// deterministic per-instance sequence (characterization-testable, e.g.
	// tennis AI); 0 seeds from GUID entropy on first execute. There is no
	// central engine RNG to defer to.
	//==========================================================================
	u_int64 XorShift64Star(u_int64& ulState)
	{
		ulState ^= ulState >> 12;
		ulState ^= ulState << 25;
		ulState ^= ulState >> 27;
		return ulState * 0x2545F4914F6CDD1DULL;
	}

	class Zenith_GraphNode_RandomBase : public Zenith_GraphNode
	{
	protected:
		u_int64 NextRandom(int32_t iSeed)
		{
			if (m_ulState == 0)
			{
				m_ulState = iSeed != 0 ? static_cast<u_int64>(static_cast<u_int32>(iSeed)) : Zenith_GUID().m_uGUID;
				if (m_ulState == 0)
				{
					m_ulState = 1;	// xorshift dies on the all-zero state
				}
			}
			return XorShift64Star(m_ulState);
		}

	private:
		u_int64 m_ulState = 0;
	};

	// Uniform float in [min, max] -> result var.
	class Zenith_GraphNode_RandomFloat : public Zenith_GraphNode_RandomBase
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_RandomFloat)
	public:
		ZENITH_PROPERTY(float, m_fMin, 0.0f)
		ZENITH_PROPERTY(float, m_fMax, 1.0f)
		ZENITH_PROPERTY(int32_t, m_iSeed, 0)
		ZENITH_PROPERTY(std::string, m_strResultVar, "random")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// 24 mantissa-safe bits -> [0, 1).
			const float fUnit = static_cast<float>(NextRandom(m_iSeed) >> 40) * (1.0f / 16777216.0f);
			Zenith_PropertyValue xResult;
			xResult.SetFloat(m_fMin + (m_fMax - m_fMin) * fUnit);
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "RandomFloat"; }
	};

	// Uniform int in [min, max] INCLUSIVE -> result var. min > max = FAILURE.
	class Zenith_GraphNode_RandomInt : public Zenith_GraphNode_RandomBase
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_RandomInt)
	public:
		ZENITH_PROPERTY(int32_t, m_iMin, 0)
		ZENITH_PROPERTY(int32_t, m_iMax, 9)
		ZENITH_PROPERTY(int32_t, m_iSeed, 0)
		ZENITH_PROPERTY(std::string, m_strResultVar, "random")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (m_iMin > m_iMax)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const u_int64 ulRange = static_cast<u_int64>(static_cast<int64_t>(m_iMax) - static_cast<int64_t>(m_iMin)) + 1;
			Zenith_PropertyValue xResult;
			xResult.SetInt32(m_iMin + static_cast<int32_t>(NextRandom(m_iSeed) % ulRange));
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "RandomInt"; }
	};
}

void Zenith_RegisterEngineGraphNodes_Math()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	xRegistry.RegisterNodeType<Zenith_GraphNode_GetListCount>("GetListCount", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_GetListElement>("GetListElement", GRAPH_EVENT_NONE, 1, false, "Blackboard");

	xRegistry.RegisterNodeType<Zenith_GraphNode_AddBlackboardInt>("AddBlackboardInt", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_AddBlackboardVector3>("AddBlackboardVector3", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_CompareBlackboardEntity>("CompareBlackboardEntity", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_MathBlackboardFloat>("MathBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Maths");
	xRegistry.RegisterNodeType<Zenith_GraphNode_MathBlackboardVector3>("MathBlackboardVector3", GRAPH_EVENT_NONE, 1, false, "Maths");
	xRegistry.RegisterNodeType<Zenith_GraphNode_LerpBlackboardFloat>("LerpBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Maths");
	xRegistry.RegisterNodeType<Zenith_GraphNode_LerpBlackboardVector3>("LerpBlackboardVector3", GRAPH_EVENT_NONE, 1, false, "Maths");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ClampBlackboardFloat>("ClampBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Maths");
	xRegistry.RegisterNodeType<Zenith_GraphNode_RandomFloat>("RandomFloat", GRAPH_EVENT_NONE, 1, false, "Maths");
	xRegistry.RegisterNodeType<Zenith_GraphNode_RandomInt>("RandomInt", GRAPH_EVENT_NONE, 1, false, "Maths");
}

#include "EntityComponent/Zenith_GraphNode_Registration_Math.Tests.inl"
