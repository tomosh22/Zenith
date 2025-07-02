#pragma once

template<typename... Args>
class Zenith_CaptureList
{
public:
	Zenith_CaptureList(Args... args)
	{
		m_xArgs = std::make_tuple(std::forward<Args>(args)...);
	}

	template<u_int u>
	auto Get()
	{
		return std::get<u>(m_xArgs);
	}

private:
	std::tuple<Args...> m_xArgs;
};

template<typename ReturnType, typename... Args>
class Zenith_Callback_Base
{
public:
	virtual ~Zenith_Callback_Base() = default;
	virtual ReturnType Execute(Args... args) = 0;
};

template<typename ReturnType>
class Zenith_Callback : public Zenith_Callback_Base<ReturnType>
{
public:
	using Callback = ReturnType(*)();

	Zenith_Callback(Callback pfn)
		: m_pfn(pfn)
	{
	}

	ReturnType Execute() override
	{
		return m_pfn();
	}

private:
	Callback m_pfn;
};

template<typename ReturnType, typename Arg0, typename... Capture>
class Zenith_Callback_ParamAndCapture : public Zenith_Callback_Base<ReturnType, Arg0>
{
public:
	using Callback = ReturnType(*)(Arg0, Zenith_CaptureList<Capture...>&);

	Zenith_Callback_ParamAndCapture(Callback pfn, Zenith_CaptureList<Capture...> xCapture)
		: m_pfn(pfn), m_xCapture(xCapture)
	{
	}

	ReturnType Execute(Arg0 arg0) override
	{
		return m_pfn(arg0, m_xCapture);
	}

private:
	Callback m_pfn;
	Zenith_CaptureList<Capture...> m_xCapture;
};