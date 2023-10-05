# Test that vector constructors work as expected.
@bp_test_no_allocations(Vec{5, Float32}(), Vec(0f0, 0f0, 0f0, 0f0, 0f0))
@bp_test_no_allocations(Vec(1, 2, 3) isa Vec{3, Int}, true)
@bp_test_no_allocations(Vec(1, 2.0, UInt8(3)) isa Vec{3, Float64}, true)
@bp_test_no_allocations(Vec{Float32}(1, -2, 3) isa Vec{3, Float32}, true)
@bp_test_no_allocations(Vec(i -> i + 2.0, 3), Vec(3.0, 4.0, 5.0))
@bp_test_no_allocations(Vec{3, Float64}(i -> i + 2) isa Vec{3, Float64}, true)
@bp_test_no_allocations(Vec{3, Float64}(i -> i + 2), Vec(3, 4, 5), true)
@bp_test_no_allocations(v3f(Val(5)), v3f(5, 5, 5))

# Test vappend.
@bp_test_no_allocations(vappend(Vec(1, 2, 3), Vec(6, 5, 4, 3, 2)),
                        Vec(1, 2, 3, 6, 5, 4, 3, 2))
@bp_test_no_allocations(vappend(Vec(1.0, 2, 3), 5) isa Vec{4, Float64}, true)

# Sometimes the size of tuples is more than the sum of their parts
#    (maybe it's an alignment thing?)
# Make sure vectors are sized as expected.
for comp_type in ALL_REALS
    for comp_size in 1:16
        T = Vec{comp_size, comp_type}
        actual_size = sizeof(T)
        expected_size = sizeof(comp_type) * comp_size
        @bp_check(actual_size == expected_size,
                  "Expected $T to have byte-size $expected_size but it's $actual_size")
    end
end

# Test our ability to construct aliases.
@bp_test_no_allocations(typeof(Vec3{Int8}(1, 2, 3)),
                        Vec{3, Int8})
@bp_test_no_allocations(typeof(v3u(3, 4, 5)),
                        Vec{3, UInt32})
@bp_test_no_allocations(typeof(Vec(UInt8(2), UInt8(5), UInt8(10))),
                        Vec{3, UInt8})

# Test type promotion.
for (nt1, nt2) in Iterators.product(ALL_REALS, ALL_REALS)
    @bp_test_no_allocations(promote_type(Vec2{nt1}, Vec2{nt2}),
                            Vec2{promote_type(nt1, nt2)})
end

# Test convert/reinterpret.
@bp_test_no_allocations(typeof(convert(v3u, Vec(2.0, 5.0, 10.0))),
                        v3u)
@bp_test_no_allocations(convert(v3u, Vec(2.0, 5.0, 10.0)),
                        v3u(2, 5, 10))
@bp_test_no_allocations(typeof(reinterpret(v3u, v3i(4, 5, 6))),
                        v3u)
@bp_test_no_allocations(reinterpret(v3u, v3i(4, 5, 6)),
                        v3u(4, 5, 6))
@bp_test_no_allocations(reinterpret(v3u, v3i(-4, 5, -6)),
                        Vec(-UInt32(4), UInt32(5), -UInt32(6)))

# Test properties.
@bp_test_no_allocations(Vec(2, 3, 4).x, 2)
@bp_test_no_allocations(Vec(2, 3, 4).y, 3)
@bp_test_no_allocations(Vec(2, 3, 4).z, 4)
@bp_test_no_allocations(Vec(2, 3, 4, 5).w, 5)
@bp_test_no_allocations(Vec(2, 3, 4)[1], 2)
@bp_test_no_allocations(Vec(2, 3, 4)[2], 3)
@bp_test_no_allocations(Vec(2, 3, 4)[3], 4)
@bp_test_no_allocations(Vec(2, 3, 4, 5)[4], 5)
@bp_test_no_allocations(Vec(2, 3, 4).r, 2)
@bp_test_no_allocations(Vec(2, 3, 4).g, 3)
@bp_test_no_allocations(Vec(2, 3, 4).b, 4)
@bp_test_no_allocations(Vec(2, 3, 4, 5).a, 5)
@bp_test_no_allocations(Vec(2, 3, 4, 5).data, (2, 3, 4, 5))
@bp_test_no_allocations(Vec(2, 3, 4, 5)[2:4], Vec(3, 4, 5))
@bp_test_no_allocations(Vec(2, 3, 4, 5)[Val(2:4)], Vec(3, 4, 5))

# Test equality.
@bp_test_no_allocations(Vec(1, 2, 3), Vec(1, 2, 3), true)
@bp_test_no_allocations(Vec(1, 2, 3), Vec(1.0, 2.0, 3.00), true)
@bp_test_no_allocations(Vec(1, 2, 3), Vec(1, 2.0, 3), true)
@bp_test_no_allocations(isapprox(Vec(1, 2, 3, 4, 5, 6),
                                 Vec(1, 2, 3, 4, 5, 6)),
                        true)
@bp_test_no_allocations(isapprox(Vec(1, 2, 3, 4, 5, 6),
                                 Vec(1, 0, 3, 4, 5, 6)),
                        false)
@bp_test_no_allocations(isapprox(Vec(1, 2, 3, 4, 5, 6),
                                 Vec(1, 2, 3, 4, 5, 0)),
                        false)
@bp_test_no_allocations(isapprox(Vec(1, 2, 3, 4, 5, 6),
                                 Vec(0, 2, 3, 4, 5, 6)),
                        false)
@bp_test_no_allocations(isapprox(Vec(1.0, 2, 3, 4, 5, 6),
                                 Vec(ntuple(i->nextfloat(Float64(i)), 6))),
                        true)
@bp_test_no_allocations(isapprox(Vec(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
                                 Vec(1.4, 2.4, 2.6, 4.4, 4.6, 5.75)),
                        false)
@bp_test_no_allocations(isapprox(Vec(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
                                 Vec(1.4, 2.4, 2.6, 4.4, 4.6, 5.75),
                                 0.5),
                        true)
@bp_test_no_allocations(isapprox(Vec(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
                                 Vec(1.0, 2.0, 3.0, 4.4, 5.0, 6.0),
                                 0.15),
                        false)
@bp_test_no_allocations(Vec(2, 2, 2, 2), 2)
@bp_test_no_allocations(2, Vec(2, 2, 2, 2))
@bp_test_no_allocations(Vec(2, 3, 4, 5) == 2, false)
@bp_test_no_allocations(2 == Vec(5, 4, 3, 2), false)

# Test arithmetic.
@bp_test_no_allocations(Vec(1, 2, 3, 4, 5) + 2, Vec(3, 4, 5, 6, 7))
@bp_test_no_allocations(Vec(1, 2, 3, 4, 5) - 2, Vec(-1, 0, 1, 2, 3))
@bp_test_no_allocations(Vec(1, 2, 3, 4, 5) * 1, Vec(1, 2, 3, 4, 5))
@bp_test_no_allocations(Vec(1, 2, 3, 4, 5) * 2, Vec(2, 4, 6, 8, 10))
@bp_test_no_allocations(isapprox(Vec(1, 2, 3, 4, 5) / 2,
                                 Vec(0.5, 1, 1.5, 2, 2.5)),
                        true)
@bp_test_no_allocations(Vec(1, 2, 3, 4, 5) ÷ 3, Vec(0, 0, 1, 1, 1))
@bp_test_no_allocations(2 + Vec(1, 2, 3, 4, 5), Vec(3, 4, 5, 6, 7))
@bp_test_no_allocations(2 - Vec(1, 2, 3, 4, 5), Vec(1, 0, -1, -2, -3))
@bp_test_no_allocations(2 * Vec(1, 2, 3, 4, 5), Vec(2, 4, 6, 8, 10))
@bp_test_no_allocations(isapprox(2 / Vec(1, 2, 3, 4, 5),
                                 Vec(2.0, 1.0, 0.6666666666666, 0.5, 0.4)),
                        true)
@bp_test_no_allocations(3 ÷ Vec(1, 2, 3, 4, 5), Vec(3, 1, 1, 0, 0))
@bp_test_no_allocations(Vec(1, 2, 3, 4, 5) + Vec(7, 5, 3, 6, 4), Vec(8, 7, 6, 10, 9))
@bp_test_no_allocations(Vec(1, 2, 3, 4, 5) - Vec(7, 5, 3, 6, 4), Vec(-6, -3, 0, -2, 1))
@bp_test_no_allocations(Vec(1, 2, 3, 4, 5) * Vec(2, 3, 4, 5, 6), Vec(2, 6, 12, 20, 30))
@bp_test_no_allocations(isapprox(Vec(1, 2, 3, 4, 5) / Vec(2, 4, 5, 12, 4),
                                 Vec(0.5, 0.5, 0.6, 0.3333333333333333333333333, 1.25)),
                        true)
@bp_test_no_allocations(Vec(5, 4, 3, 2, 1) ÷ Vec(1, 2, 3, 4, 5), Vec(5, 2, 1, 0, 0))
@bp_test_no_allocations(-Vec(2, -3, 4, -5, 6, -7), Vec(-2, 3, -4, 5, -6, 7))

# Test boolean vector operations.
@bp_test_no_allocations(Vec(true, false) | Vec(false, true),
                        Vec(true, true))
@bp_test_no_allocations(Vec(true, false) | false,
                        Vec(true, false))
@bp_test_no_allocations(true | Vec(true, false),
                        Vec(true, true))
@bp_test_no_allocations(Vec(true, false, true) & Vec(true, true, false),
                        Vec(true, false, false))
@bp_test_no_allocations(Vec(true, false, true) & false,
                        Vec(false, false, false))
@bp_test_no_allocations(Vec(true, false, true) & true,
                        Vec(true, false, true))
@bp_test_no_allocations(vselect(Vec(1, 2, 3), Vec(4, 5, 6), Vec(true, false, true)),
                        Vec(4, 2, 6))

# Test vindex():
@bp_test_no_allocations(vindex(Vec(2, 3, 4), Vec(5, 6, 7)),
                        2 + (2*5) + (3*(5*6)))
@bp_test_no_allocations(vindex(2 + (2*5) + (3*(5*6)), Vec(5, 6, 7)),
                        Vec(2, 3, 4))

# Test comparisons and all/any.
@bp_test_no_allocations(Vec(1, 2, 3) < 2, Vec(true, false, false))
@bp_test_no_allocations(Vec(1, 2, 3) <= 2, Vec(true, true, false))
@bp_test_no_allocations(Vec(1, 2, 3) > 2, Vec(false, false, true))
@bp_test_no_allocations(Vec(1, 2, 3) >= 2, Vec(false, true, true))
@bp_test_no_allocations(2 > Vec(1, 2, 3), Vec(true, false, false))
@bp_test_no_allocations(2 >= Vec(1, 2, 3), Vec(true, true, false))
@bp_test_no_allocations(2 < Vec(1, 2, 3), Vec(false, false, true))
@bp_test_no_allocations(2 <= Vec(1, 2, 3), Vec(false, true, true))
@bp_test_no_allocations(Vec(1, 2, 3) < Vec(2, 2, 2), Vec(true, false, false))
@bp_test_no_allocations(Vec(1, 2, 3) <= Vec(2, 2, 2), Vec(true, true, false))
@bp_test_no_allocations(Vec(1, 2, 3) > Vec(2, 2, 2), Vec(false, false, true))
@bp_test_no_allocations(Vec(1, 2, 3) >= Vec(2, 2, 2), Vec(false, true, true))
@bp_test_no_allocations(all(Vec(true, false)), false)
@bp_test_no_allocations(all(Vec(true, true)), true)
@bp_test_no_allocations(any(Vec(false, true)), true)
@bp_test_no_allocations(any(Vec(false, false, false, false)), false)

# Test setfield.
@bp_test_no_allocations(@set(Vec(1, 2, 3).x = 3),
                        Vec(3, 2, 3))
@bp_test_no_allocations(@set(Vec(1, 2, 3)[1] = 3),
                        Vec(3, 2, 3))
@bp_test_no_allocations(@set(Vec(1, 2, 3).data = (4, 5, 6)),
                        Vec(4, 5, 6))
# @bp_test_no_allocations(@set(Vec(1, 2, 3).data = (4, 5, 6.0)),
#                         Vec(4, 5, 6))
V::v3u = v3u(3, 4, 5)
@bp_test_no_allocations(begin
                            global V
                            @set! V.z = 7
                            V
                        end,
                        v3u(3, 4, 7))

# Test number stuff.
@bp_test_no_allocations(typemin(Vec{3, UInt8}),
                        Vec(UInt8(0), UInt8(0), UInt8(0)))
@bp_test_no_allocations(typemax(v2f), Vec(+Inf, +Inf))
@bp_test_no_allocations(min(Vec(5, 6, -11, 8, 1)), -11)
@bp_test_no_allocations(min(Vec(3, 4), Vec(1, 10)),
                        Vec(1, 4))
@bp_test_no_allocations(min(Vec(3, 5), 4),
                        Vec(3, 4))
@bp_test_no_allocations(min(4, Vec(3, 5)),
                        Vec(3, 4))
@bp_test_no_allocations(max(Vec(5, 6, -11, 8, 1)), 8)
@bp_test_no_allocations(max(Vec(3, 4), Vec(1, 10)),
                        Vec(3, 10))
@bp_test_no_allocations(max(Vec(3, 5), 4),
                        Vec(4, 5))
@bp_test_no_allocations(max(4, Vec(3, 5)),
                        Vec(4, 5))
@bp_test_no_allocations(minmax(Vec(3, 4), Vec(1, 10)),
                        (Vec(1, 4), Vec(3, 10)))
@bp_test_no_allocations(minmax(3, Vec(1, 10)),
                        (Vec(1, 3), Vec(3, 10)))
@bp_test_no_allocations(minmax(Vec(1, 10), 3),
                        (Vec(1, 3), Vec(3, 10)))
@bp_test_no_allocations(abs(Vec(-1.5, 2, -20, 5.44)),
                        Vec(1.5, 2, 20, 5.44))
@bp_test_no_allocations(clamp(Vec(3, 7), 3, 5),
                        Vec(3, 5))
@bp_test_no_allocations(clamp(Vec(3, 7), Vec(1, 10), Vec(2, 15)),
                        Vec(2, 10))

# Test the dot product.
const ⋅ = Bplus.Math.:⋅  # It's also defined by the Images package
@bp_test_no_allocations(vdot(Vec(1, 2), Vec(4, 5)),  14)
@bp_test_no_allocations(Vec(1, 2) ⋅ Vec(4, 5), vdot(Vec(1, 2), Vec(4, 5)))

# Test swizzling.
#TODO: I'm pretty certain these don't allocate, but for some reason they appear to in the test.
function swizzle_test()
    return tuple(
        Vec(1, 2, 3, 4).xyz,
        Vec(1, 2, 3, 4).xyz0,
        Vec(1, 2, 3, 4).xyz1,
        Vec(1, 2, 3, 4).xyzΔ,
        Vec(1, 2, 3, 4).xyz∇,
        v2f(1, 2).xxxΔ,
        v2f(3, 4).xxx∇,
        Vec(1, 2, 3, 4).xxyyz01∇Δw
    )
end
swizzle_test()
@bp_check(swizzle_test() == tuple(
    Vec(1, 2, 3),
    Vec(1, 2, 3, 0),
    Vec(1, 2, 3, 1),
    Vec(1, 2, 3, typemax(Int)),
    Vec(1, 2, 3, typemin(Int)),
    Vec(1, 1, 1, typemax_finite(Float32)),
    Vec(3, 3, 3, typemin_finite(Float32)),
    Vec(1, 1, 2, 2, 3, 0, 1, typemin(Int), typemax(Int), 4)
))
@bp_test_no_allocations(v4i(3, 2, 1, 4)[2, 3, 1, 1, 3, 4, 1],
                        VecI{7}(2, 1, 3, 3, 1, 4, 3),
                        "Swizzling via getindex")

@bp_test_no_allocations(typeof(rand(v3f)), v3f)
@bp_test_no_allocations(typeof(rand(v4u)), v4u)

# Test array-like behavior.
@bp_test_no_allocations(map(f->f*f, Vec(1, 2, 3, 4)) === Vec(1, 4, 9, 16),
                        true)
@bp_test_no_allocations(map(==, Vec(1, 2, 3), Vec(1, 2, 3)), Vec(true, true, true))
@bp_test_no_allocations(map(==, Vec(1, 2, 3), Vec(3, 2, 1)), Vec(false, true, false))
@bp_test_no_allocations(Vec(3, 4, 5)[3], 5)
@bp_test_no_allocations(Vec(3, 4, 5, 7, 3, 1)[end], 1)
@bp_test_no_allocations(sum(Vec(3, 4, 5)), 3+4+5)
@bp_test_no_allocations(reduce(-, Vec(2, 4, 6)), 2-4-6)
@bp_test_no_allocations(foldl(-, Vec(1, 2, 3)), 1-2-3)
@bp_test_no_allocations(foldr(-, Vec(6, 7, 8)), 8-7-6)

# Test vsize().
@bp_test_no_allocations_setup(
    arr::Matrix{Float64} = [ 3.0  6.0  7.0
                             1.4  3.1  -1.0 ],
    vsize(arr),
    Vec(2, 3)
)

# Test VecRange/the colon operator.
@bp_check(collect(Bplus.Math.VecRange(Vec(1), Vec(5), Vec(2))) ==
            [ 1, 3, 5 ],
          "collect(1:Vec(2):5) == ", collect(Bplus.Math.VecRange(Vec(1), Vec(5), Vec(2))))
@bp_test_no_allocations(Vec(1, 1) : Vec(10, 20),
                        Bplus.Math.VecRange(Vec(1, 1), Vec(10, 20), Vec(1, 1)))
@bp_test_no_allocations(Vec(1, 1) : Vec(2, 4) : Vec(10, 20),
                        Bplus.Math.VecRange(Vec(1, 1), Vec(10, 20), Vec(2, 4)))
@bp_test_no_allocations(1 : Vec(10, 20),
                        Bplus.Math.VecRange(Vec(1, 1), Vec(10, 20), Vec(1, 1)))
@bp_test_no_allocations(UInt8(1) : Vec(10, 20),
                        Bplus.Math.VecRange(Vec(1, 1), Vec(10, 20), Vec(1, 1)))
@bp_test_no_allocations(1 : 2 : Vec(11, 21),
                        Bplus.Math.VecRange(Vec(1, 1), Vec(11, 21), Vec(2, 2)))
@bp_test_no_allocations(1 : 2 : Vec(11, 21),
                        Bplus.Math.VecRange(Vec(1, 1), Vec(11, 21), Vec(2, 2)))
function f()::Int
    i::Int = 0
    for p::v2i in v2i(1, 2) : v2i(30, 2)
        i += p.x
    end
    return i
end
@bp_test_no_allocations(v3i(4, 5, 6) in 1:v3i(6, 6, 6), true)
@bp_test_no_allocations(v3i(4, 5, 6) in 1:6, true)
@bp_test_no_allocations(v3i(4, 5, 6) in 4:v3i(6, 6, 6), true)
@bp_test_no_allocations(v3i(4, 5, 6) in 5:v3i(6, 6, 6), false)
@bp_test_no_allocations(v3i(5, 4, 6) in 5:v3i(6, 6, 6), false)
@bp_test_no_allocations(v3i(5, 4, 6) in 5:6, false)
@bp_test_no_allocations(v3i(6, 5, 4) in 5:v3i(6, 6, 6), false)
@bp_test_no_allocations(v3i(4, 5, 6) in 7:v3i(1, 1, 1), false)
@bp_test_no_allocations(v3i(4, 5, 6) in 7:1, false)
@bp_test_no_allocations(v3i(4, 5, 6) in one(v3i):v3i(3, 2, 1):v3i(6, 6, 6), true)
@bp_test_no_allocations(v3i(4, 5, 6) in one(v3i):v3i(3, 2, 1):v3i(4, 5, 6), true)
@bp_test_no_allocations(v3i(4, 5, 6) in one(v3i):v3i(1, 2, 3):v3i(4, 5, 6), false)
#TODO: In the REPL this doesn't seem to allocate, but here it does?
#@bp_test_no_allocations(f(), sum(1:30))
@bp_check(f() == sum(1:30))

# Test map!() on a range of coordinates, which invokes several helper functions that I had to overload.
const MAP_RANGE = 1:v2i(2, 3)
map_data::Vector{v2i} = fill(zero(v2i), prod(last(MAP_RANGE)))
map!(v -> v*10, map_data, 1:v2i(2, 3))
@bp_check(map_data == [ v2i(10, 10), v2i(20, 10),
                        v2i(10, 20), v2i(20, 20),
                        v2i(10, 30), v2i(20, 30) ],
          "Actual: ", map_data)

# Test random values inside vector ranges.
@bp_test_no_allocations(typeof(rand(1:v3i(4, 5, 6))), v3i)
for _ in 1:100
    range = v3i(1, 5, 9):v3i(3, 2, 1):v3i(100, 2000, 400)
    val = rand(range)
    @bp_test_no_allocations(val in range, true,
                            val, " not in ", range)
end

# Test vdist_sqr and vdist:
@bp_test_no_allocations(vdist_sqr(Vec(1, 1, 1), Vec(3, 1, 1)), 4)
@bp_test_no_allocations(vdist_sqr(Vec(1, 1, 1), Vec(1, 3, 1)), 4)
@bp_test_no_allocations(vdist_sqr(Vec(1, 1, 1), Vec(1, 1, 3)), 4)
@bp_test_no_allocations(vdist_sqr(Vec(1, 1, 1), Vec(3, 3, 3)), 12)
@bp_test_no_allocations(isapprox(vdist(Vec(1, 1, 1), Vec(2, 0, 2)),
                                 sqrt(3.0)),
                        true)

# Test vlength_sqr and vlength:
@bp_test_no_allocations(vlength_sqr(Vec(5, 0, 0)), 25)
@bp_test_no_allocations(vlength_sqr(Vec(0, 5, 0)), 25)
@bp_test_no_allocations(vlength_sqr(Vec(0, 0, 5)), 25)
@bp_test_no_allocations(vlength_sqr(Vec(5, 5, 5)), 75)
@bp_test_no_allocations(isapprox(vlength(Vec(1, -1, -1)),
                                 sqrt(3.0)),
                        true)

# Test vnorm and v_is_normalized:
@bp_test_no_allocations(isapprox(vnorm(Vec(1, -1)),
                                 1/Vec(sqrt(2.0), -sqrt(2.0))),
                        true)
@bp_test_no_allocations(isapprox(vnorm(Vec(1, -1, 1)),
                                 1/Vec(sqrt(3.0), -sqrt(3.0), sqrt(3.0))),
                        true)
@bp_test_no_allocations(v_is_normalized(1/Vec(sqrt(2.0), -sqrt(2.0))),
                        true)
@bp_test_no_allocations(v_is_normalized(1/Vec(sqrt(2.1), -sqrt(2.0))),
                        false)

# Test vcross:
@bp_test_no_allocations(vcross(Vec(1, 0, 0), Vec(0, 1, 0)),
                        Vec(0, 0, 1))
@bp_test_no_allocations(Vec(1, 0, 0) × Vec(0, 1, 0),
                        Vec(0, 0, 1))
@bp_test_no_allocations(Vec(0, 0, 1) × Vec(0, 1, 0),
                        Vec(-1, 0, 0))

# Test the "up axis" stuff.
@bp_test_no_allocations(get_up_vector(Int8), Vec(0, 0, 1))
@bp_test_no_allocations(get_horz(Vec(3, 4, 5)), Vec(3, 4))
Bplus.Math.get_up_axis()::Int = 1
Bplus.Math.get_up_sign()::Int = -1
@bp_test_no_allocations(get_up_vector(Int16), Vec(-1, 0, 0))
@bp_test_no_allocations(get_horz(Vec(3, 4, 5)), Vec(4, 5))
# Undo the change to the "up" axis.
Bplus.Math.get_up_axis()::Int = 3
Bplus.Math.get_up_sign()::Int = 1
@bp_test_no_allocations(get_vert(Vec(3, 4, 5)), 5)
@bp_test_no_allocations(to_3d(Vec(3, 4)), Vec(3, 4, 0))
@bp_test_no_allocations(to_3d(Vec(3, 4), 5), Vec(3, 4, 5))