"Gets whether a given expression is a function call"
is_function_call(expr) = (
    if isexpr(expr, :call)
        true
    elseif isexpr(expr, :(::))
        is_function_call(expr.args[1])
    elseif isexpr(expr, :where)
        is_function_call(expr.args[1])
    else
        false
    end
)

"Gets whether a given expression is a function definition"
is_function_def(expr) = (
    if isexpr(expr, :function)
        true
    elseif isexpr(expr, :(=))
        is_function_call(expr.args[1])
    else
        false
    end
)

"Gets whether a given expression is a struct field declaration"
is_field_def(expr) = (
    if expr isa Symbol
        true
    elseif isexpr(expr, :(::))
        expr.args[1] isa Symbol
    elseif isexpr(expr, :(=))
        is_field_def(expr.args[1])
    else
        false
    end
)

export is_function_call, is_function_def, is_field_def