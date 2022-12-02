#ifndef PVXS_IOCSHCOMMAND_H
#define PVXS_IOCSHCOMMAND_H

namespace pvxs {
namespace ioc {

template<typename E>
struct Arg;

template<>
struct Arg<int> {
	static constexpr iocshArgType code = iocshArgInt;
	static int get(const iocshArgBuf& buf) {
		return buf.ival;
	}
};

template<>
struct Arg<double> {
	static constexpr iocshArgType code = iocshArgDouble;
	static double get(const iocshArgBuf& buf) {
		return buf.dval;
	}
};

template<>
struct Arg<const char*> {
	static constexpr iocshArgType code = iocshArgString;
	static const char* get(const iocshArgBuf& buf) {
		return buf.sval;
	}
};



// index_sequence from:
//http://stackoverflow.com/questions/17424477/implementation-c14-make-integer-sequence

template<std::size_t ... I>
struct index_sequence {
	using type = index_sequence;
	using value_type = std::size_t;
	static constexpr std::size_t size() {
		return sizeof ... (I);
	}
};

template<typename Seq1, typename Seq2>
struct concat_sequence;

template<std::size_t ... I1, std::size_t ... I2>
struct concat_sequence<index_sequence<I1 ...>, index_sequence<I2 ...> > : public index_sequence<I1 ...,
                                                                                                (sizeof ... (I1)
		                                                                                                + I2) ...> {
};

template<std::size_t I>
struct make_index_sequence : public concat_sequence<typename make_index_sequence<I / 2>::type,
                                                    typename make_index_sequence<I - I / 2>::type> {
};

template<>
struct make_index_sequence<0> : public index_sequence<> {
};

template<>
struct make_index_sequence<1> : public index_sequence<0> {
};

template<typename T>
using ConstString = const char*;

/**
 * Class that encapsulates an IOC command.
 * The command has a name and a description.
 *
 * A method allows you to register the implementation of the command.
 *
 * The constructor takes the name of the function and the help text
 * Call implementation() with a reference to your implementation function to complete registration
 * e.g.:
 * 	    pvxs::ioc::IOCShRegister<int>("pvxsl", "detailed help text").implementation<&pvxsl>();
 * @tparam IOCShFunctionArgumentTypes the list of 0 or more argument types for the shell function to be registered
 */
template<typename ...IOCShFunctionArgumentTypes>
class IOCShCommand {
private:
	// The name of the shell command
	const char* const name;
	// The description of the shell command as shown in help
	// An array of strings for each of the required arguments, as many as the argument types specified
	const char* const argumentNames[1 + sizeof...(IOCShFunctionArgumentTypes)];

public:
	// All shell commands return void and take a variable number of arguments of any supported type
	using IOCShFunction = void (*)(IOCShFunctionArgumentTypes...);

	// Construct a new IOC shell command with a name and description
	 constexpr explicit IOCShCommand(const char* name, ConstString<IOCShFunctionArgumentTypes>... descriptions);

	// Register a shell command's implementation
	template<IOCShFunction> void implementation();

	// Internal utility functions used while registering and implementing the shell function
	template<IOCShFunction, size_t... Idxs> void call(const iocshArgBuf* args);

	// Internal utility functions used while registering and implementing the shell function
	template<IOCShFunction, size_t... Idxs> void implement(index_sequence<Idxs...>);
};

} // pvxs
} // ioc

#endif //PVXS_IOCSHCOMMAND_H
