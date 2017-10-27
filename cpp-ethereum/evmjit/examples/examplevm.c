#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <evm.h>


struct examplevm
{
    struct evm_instance instance;
    const struct evm_host* host;

    int example_option;
};

static void evm_destroy(struct evm_instance* evm)
{
    free(evm);
}

/// Example options.
///
/// VMs are allowed to omit this function implementation.
int evm_set_option(struct evm_instance* instance,
                   char const* name,
                   char const* value)
{
    struct examplevm* vm = (struct examplevm*)instance;
    if (strcmp(name, "example-option") == 0) {
        long int v = strtol(value, NULL, 0);
        if (v > INT_MAX || v < INT_MIN)
            return 0;
        vm->example_option = (int)v;
        return 1;
    }

    return 0;
}

static void evm_release_result(struct evm_result const* result)
{
    (void)result;
}

static void free_result_output_data(struct evm_result const* result)
{
    free((uint8_t*)result->output_data);
}

static struct evm_result execute(struct evm_instance* instance,
                                 struct evm_context* context,
                                 enum evm_revision rev,
                                 const struct evm_message* msg,
                                 const uint8_t* code,
                                 size_t code_size)
{
    struct evm_result ret = {};
    if (code_size == 0) {
        // In case of empty code return a fancy error message.
        const char* error = rev == EVM_BYZANTIUM ?
                            "Welcome to Byzantium!" : "Hello Ethereum!";
        ret.output_data = (const uint8_t*)error;
        ret.output_size = strlen(error);
        ret.code = EVM_FAILURE;
        ret.release = NULL;  // We don't need to release the constant messages.
        return ret;
    }

    struct examplevm* vm = (struct examplevm*)instance;

    // Simulate executing by checking for some code patterns.
    // Solidity inline assembly is used in the examples instead of EVM bytecode.

    // Assembly: `{ mstore(0, address()) return(0, msize()) }`.
    const char return_address[] = "30600052596000f3";

    // Assembly: `{ sstore(0, add(sload(0), 1)) }`
    const char counter[] = "600160005401600055";

    if (code_size == strlen(return_address) &&
        strncmp((const char*)code, return_address, code_size)) {
        static const size_t address_size = sizeof(msg->address);
        uint8_t* output_data = (uint8_t*)malloc(address_size);
        if (!output_data) {
            // malloc failed, report internal error.
            ret.code = EVM_INTERNAL_ERROR;
            return ret;
        }
        memcpy(output_data, &msg->address, address_size);
        ret.code = EVM_SUCCESS;
        ret.output_data = output_data;
        ret.output_size = address_size;
        ret.release = &free_result_output_data;
        return ret;
    }
    else if (code_size == strlen(counter) &&
        strncmp((const char*)code, counter, code_size)) {
        struct evm_uint256be value;
        const struct evm_uint256be index = {{0,}};
        vm->host->get_storage(&value, context, &msg->address, &index);
        value.bytes[31] += 1;
        vm->host->set_storage(context, &msg->address, &index, &value);
        ret.code = EVM_SUCCESS;
        return ret;
    }

    ret.release = evm_release_result;
    ret.code = EVM_FAILURE;
    ret.gas_left = 0;

    return ret;
}

static struct evm_instance* evm_create(const struct evm_host* host)
{
    struct examplevm* vm = calloc(1, sizeof(struct examplevm));
    struct evm_instance* interface = &vm->instance;
    interface->destroy = evm_destroy;
    interface->execute = execute;
    interface->set_option = evm_set_option;
    vm->host = host;
    return interface;
}

struct evm_factory examplevm_get_factory()
{
    struct evm_factory factory = {EVM_ABI_VERSION, evm_create};
    return factory;
}
