module Utilities

using StaticArrays, MacroTools, Dates, StructTypes

include("asserts.jl")
@make_toggleable_asserts bp_utils_

include("basic.jl")
include("unions.jl")
include("macros.jl")

include("strings.jl")
include("enums.jl")

include("up_to.jl")

include("prng.jl")
include("rand_iterator.jl")

end