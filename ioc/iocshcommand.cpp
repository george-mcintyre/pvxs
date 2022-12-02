#include <stdexcept>
#include <iocsh.h>
#include <iocshcommand.h>

namespace pvxs {
namespace ioc {

// Construct a new IOC shell command with a name and description
template<typename ...IOCShFunctionArgumentTypes>
constexpr IOCShCommand<IOCShFunctionArgumentTypes...>::IOCShCommand(
		const char* name, ConstString<IOCShFunctionArgumentTypes>... descriptions)
		:name(name),
		 argumentNames{ descriptions..., 0 } {
}

// Create an implementation for this IOC command
template<typename ...IOCShFunctionArgumentTypes>
template<void (* IOCFunction)(IOCShFunctionArgumentTypes...)>
void IOCShCommand<IOCShFunctionArgumentTypes...>::implementation() {
	IOCShCommand<IOCShFunctionArgumentTypes...>::implement<IOCFunction>(
			make_index_sequence<sizeof...(IOCShFunctionArgumentTypes)>{});
}

// Implement the command by registering the callback with EPICS iocshRegister()
template<typename ...IOCShFunctionArgumentTypes>
template<void (* IOCFunction)(IOCShFunctionArgumentTypes...), size_t... Idxs>
void IOCShCommand<IOCShFunctionArgumentTypes...>::implement(index_sequence<Idxs...>) {
	static const iocshArg argumentStack[1 + sizeof...(IOCShFunctionArgumentTypes)] = {
			{
					argumentNames[Idxs],
					Arg<IOCShFunctionArgumentTypes>::code
			}...
	};
	static const iocshArg* const args[] = { &argumentStack[Idxs]..., 0 };
	static const iocshFuncDef def = { name, sizeof...(IOCShFunctionArgumentTypes), args };

	// Register the callback for the implementation of this command
	iocshRegister(&def, &call<IOCFunction, Idxs...>);
}

// The actual callback that is executed for the registered command
// The function is called with a variadic argument list of heterogeneous types based on the
// declared registration template types
// by calling the appropriate get methods on the templated Arg(s)
template<typename ...IOCShFunctionArgumentTypes>
template<void (* IOCFunction)(IOCShFunctionArgumentTypes...), size_t... Idxs>
void IOCShCommand<IOCShFunctionArgumentTypes...>::call(const iocshArgBuf* args) {
	(*IOCFunction)(Arg<IOCShFunctionArgumentTypes>::get(args[Idxs])...);
}

} // pvxs
} // ioc
