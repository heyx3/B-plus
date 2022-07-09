"
A node in a scene tree. Offers the following interface:

## Transforms

These functions can cause the node's cached data to update,
    so they return a copy of the node as well.

* `local_transform(node)::Tuple{@Mat(4, 4, F), Node}`
* `world_transform(node)::Tuple{@Mat(4, 4, F), Node}`
* `world_inverse_transform(node)::Tuple{@Mat(4, 4, F), Node}`

## Tree ops

* `set_parent(node_id, new_parent, context, preserve_space = Spaces.self)`

## Iteration
* `siblings(node, context, include_self = true)`
* `children(node, context)`
* `parents(node, context)`
* `family(node, context, include_self = true)` : uses an efficient depth-first search.
* `family_breadth_first(node, context, include_self = true)` :
        uses a breadth-first search that works well for smaller trees.
* `family_breadth_first_deep(node, context, include_self = true; buffer = NodeID[ ])` :
        uses a breadth-first search that works better for larger trees.

## Utilities
* `try_deref_node(node_id, context)::Optional{TNode}`
"
struct Node{TNodeID, F<:AbstractFloat}
    #TODO: Generalize to any number of dimensions.
    parent::TNodeID

    sibling_prev::TNodeID
    sibling_next::TNodeID

    n_children::Int
    child_first::TNodeID

    local_pos::Vec3{F}
    local_rot::Quaternion{F}
    local_scale::Vec3{F}

    is_cached_self::Bool
    cached_matrix_self::@Mat(4, 4, F)

    is_cached_world_mat::Bool
    cached_matrix_world::@Mat(4, 4, F)
    cached_matrix_world_inverse::@Mat(4, 4, F)

    is_cached_world_rot::Bool
    cached_rot_world::Quaternion{F}
end
export Node

function Node{TNodeID, F}( local_pos::Vec3 = zero(Vec3{F})
                           ;
                           local_rot::Quaternion = Quaternion{F}(),
                           local_scale::Vec3 = one(Vec3{F})
                         )::Node{TNodeID, F} where {TNodeID, F}
    return Node(null_node_id(TNodeID), null_node_id(TNodeID),
                null_node_id(TNodeID), null_node_id(TNodeID),
                0,
                convert(Vec3{F}, local_pos),
                convert(Quaternion{F}, local_rot),
                convert(Vec3{F}, local_scale),
                false, m_identity(4, 4, F),
                false, m_identity(4, 4, F), m_identity(4, 4, F),
                false, Quaternion{F}())
end
function Node{TNodeID}( local_pos::Vec3{F}
                        ;
                        local_rot::Quaternion{F} = Quaternion{F}(),
                        local_scale::Vec3{F} = one(Vec3{F})
                      )::Node{TNodeID, F} where {TNodeID, F}
    return Node{TNodeID, F}(local_pos; local_rot=local_rot, local_scale=local_scale)
end


println("#TODO: The inability to put mutable nodes in contiguous memory is hugely painful; pick one of the below ideas to alleviate it.")
# 1. Add a macro where the node data is copied into a mutable version,
#      the below interface is mostly done with the mutable version,
#      then at the end the mutable version overwrites the immutable data.
# 2. Shift to an interface that's entirely done through node ID's,
#      making the interface much more sane at the cost of much more frequent node-dereferencing
#      (but maybe that can be optimized out?)


######################
#  Public Interface  #
######################

#TODO: In a number of locations there are consecutive calls to @set! for some node data. Is this inefficient? It should be pretty obvious in the profiler.
println("#TODO: Make the 'context' optional in the public API")


##  Transform calculations  ##

"
Gets (or calculates) the local-space transform of this node.
This may involve updating the node's cached data, so a new copy of the node is also returned.

Note that the node's original storage in the context is not updated!
"
function local_transform( node::Node{TNodeID, F}
                        )::Tuple{@Mat(4, 4, F), Node{TNodeID, F}} where {TNodeID, F}
    if !node.is_cached_self
        @set! node.is_cached_self = true
        @set! node.cached_matrix_self = m4_world(node.local_pos, node.local_rot, node.local_scale)
    end

    return (node.cached_matrix_self, node)
end
"
Gets (or calculates) the world-space transform of this node.
This may involve updating the node's cached data, so a new copy of the node is also returned.

Note that the node's original storage in the context is not updated
    (though its parents may be)!
"
function world_transform( node::Node{TNodeID, F},
                          context::TContext
                        )::Tuple{@Mat(4, 4, F), Node{TNodeID, F}} where {TNodeID, F, TContext}
    if !node.cached_matrix_world
        # Get our local matrix.
        (matrix_local::@Mat(4, 4, F), node) = local_transform(node)

        # Transform it by the parent's world matrix.
        local matrix_world::@Mat(4, 4, F)
        if is_null_id(node.parent)
            matrix_world = matrix_local
        else
            parent_node::Node{TNodeID, F} = deref_node(node.parent)
            (matrix_parent_world, parent_node) = world_transform(parent_node, context)
            update_node(node.parent, context, parent_node)

            matrix_world = m_combine(matrix_local, matrix_parent_world)
        end

        # Update this node's cache, including the inverse world matrix
        #    since it's used pretty often.
        @set! node.is_cached_world_mat = true
        @set! node.cached_matrix_world = matrix_world
        @set! node.cached_matrix_world_inverse = m_invert(node.cached_matrix_world)
    end

    return (node.cached_matrix_world, node)
end
"
Gets (or calculates) the inverse-world-space transform of this node,

This may involve updating the node's cached data, so a new copy of the node is also returned.
Note that the node's original storage in the context is not updated
    (though its parents may be)!
"
function world_inverse_transform( node::Node{TNodeID, F},
                                  context::TContext
                                )::Tuple{@Mat(4, 4, F), Node{TNodeID, F}} where {TContext, TNodeID, F}
    # Make sure the cache is updated, then return the cached value.
    (_, node) = world_transform(node, context)
    return (node.cached_matrix_world_inverse, node)
end

#TODO: Various transform setters


##  Tree structure  ##

"
Changes a node's parent. You may pass a null ID to merely remove the current parent.

Preserves either the world-space or local-space transform of this node.
Preserving world-space can cause small imprecisions in local-space data,
    so local-space is the default.

Updates the data for all relevant nodes (self, old parent, new parent, etc.),
    and also returns the node's new data for reference.
"
function set_parent( node_id::TNodeID,
                     new_parent::TNodeID,
                     context::TContext,
                     preserve::E_Spaces = Spaces.self
                   )::Node{TNodeID} where {TNodeID, TContext}
    node::Node{TNodeID} = deref_node(node_id, context)
    old_parent::TNodeID = node.parent

    # Check some edge-cases:
    if new_parent == old_parent
        return node
    elseif is_deep_child_of(node_id, new_parent)
        error("Trying to create an infinite loop of node parents")
    end

    # Remember which callback to raise at the end of this, if any.
    is_becoming_rooted::Bool = is_null_id(new_parent)
    is_uprooting::Bool = !is_becoming_rooted && is_null_id(old_parent)

    # Update old parents and siblings.
    if !is_null_id(old_parent)
        disconnect_parent(node, node_id, context)
    end

    # Update new parents and siblings.
    if !is_null_id(new_parent)
        new_parent_data::Node{TNodeID} = deref_node(new_parent, context)

        # The easiest place to insert into the doubly-linked list of siblings
        #    is at the beginning of the list.

        # Update our own data.
        @set! node.sibling_next = new_parent_data.child_first
        @set! node.sibling_prev = null_node_id(TNodeID)
        # Setting 'node.parent' is done below, after position calculations.
        # The call to 'update_node()' is similarly pushed to the end of the function.

        # Update the new sibling's data.
        if !is_null_id(node.sibling_next)
            next_sibling::Node{TNodeID} = deref_node(node.sibling_next, context)
            @bp_scene_tree_assert(is_null_id(next_sibling.sibling_prev),
                                  "Inserted a node at the beginning of the child list,",
                                  " but its 'next' sibling already had a 'previous' sibling??")
            @set! next_sibling.sibling_prev = node_id
            update_node(node.sibling_next, context, next_sibling)
        end

        # Update the parent's data.
        @set! new_parent_data.child_first = node_id
        @set! new_parent_data.n_children += 1
        update_node(new_parent, context, new_parent_data)
    end

    # Process 3D transformation data for this node (and its children).
    if preserve == Spaces.self
        # Leave this node's local position alone, but change its world position.
        node = invalidate_world_space(node, context, true)
    elseif preserve == Spaces.world
        # The local transform will change, but the world transform should stay the same,
        #    which means the cached world-space data doesn't need to be invalidated.

        # Calculate the new local matrix by taking the world-space matrix,
        #    and combining it with the inverse of the new parent's world-space matrix.
        # This can intuitively be described as
        #    "undo the new parent's transformations, then apply the desired world-space transform".

        (world_mat, node) = world_transform(node, context)
        local_mat::@Mat(4, 4, F)
        if is_null_id(new_parent)
            local_mat = world_mat
        else
            (new_parent_inverse_world_mat, new_parent_data) = world_inverse_transform(new_parent_data, context)
            update_node(new_parent, context, new_parent_data)

            local_mat = m_combine(new_parent_inverse_world_mat, world_mat)
        end

        # Calculate a new local transform that preserves the world-space transform.
        @set! node.cached_matrix_self = local_mat
        @set! node.is_cached_self = true
        local_data = m_decompose(node.cached_matrix_self) #TODO: This function doesn't actually exist yet.
        @set! node.local_pos = local_data.pos
        @set! node.local_rot = local_data.rot
        @set! node.local_scale = local_data.scale
    else
        error("Unhandled case: ", preserve)
    end

    # Finish updating our node's data.
    @set! node.parent = new_parent
    update_node(node_id, context, node)

    # Raise the correct callback for what just happened.
    if is_becoming_rooted
        on_rooted(node_id, context)
    elseif is_uprooting
        on_uprooted(node_id, context)
    end

    return node
end

#TODO: Ability to move this transform within its parent list, and/or swap places with a sibling.


##  Iteration  ##

"Iterates over the siblings of a node, from start to finish, optionally ignoring the given one."
@inline siblings(node::Node, context, include_self::Bool = true) = Siblings(node, context, !include_self)
"Overload that takes a node ID instead of a `Node` instance."
@inline siblings(node_id, context, args...) = siblings(deref_node(node_id, context), context, args...)

"Iterates over the children of a node."
@inline children(node::Node, context) = Children(node, context)
"Overload that takes a node ID instead of a `Node` instance."
@inline children(node_id, context) = children(deref_node(node_id, context), context)

"Iterates over the parent, grand-parent, etc. of a node."
@inline parents(node::Node, context) = Parents(node, context)
"Overload that takes a node ID instead of a `Node` instance."
@inline parents(node_id, context) = parents(deref_node(node_id, context), context)

"
Iterates over all nodes underneath this one, in Depth-First order.
Must take the root node by its ID, not the `Node` instance.

For Breadth-First order (which is slower), see `family_breadth_first`.
"
@inline family(node_id, context, include_self::Bool = true) = FamilyDFS(node_id, context, include_self)
"
Iterates over all nodes underneath this one, in Breadth-first order.
Must take the root node by its ID, not the `Node` instance.

This search is slower, but sometimes Breadth-First is useful.
By default, a technique is used that has no heap allocations, but scales poorly for large trees.
For a heap-allocating technique that works better on large trees, see `family_breadth_first_deep`.
"
@inline family_breadth_first(node_id, context, include_self::Bool) =
    FamilyBFS_NoHeap(FamilyDFS(node_id, context, include_self))
"
Iterates over all nodes underneath this one, in Breadth-first order.
Must take the root node by its ID, not the `Node` instance.

This search is slower than Depth-First, but sometimes Breadth-First is useful.

In this version, a heap-allocated queue is used to efficiently iterate over large trees.
For smaller trees, consider using `family_breadth_first`, which will not allocate.
"
@inline function family_breadth_first_deep( node_id::TNodeID, context,
                                            include_self::Bool = true
                                            ;
                                            buffer::Vector{TNodeID} = preallocated_vector(TNodeID, 20)
                                          ) where {TNodeID}
    error("Not implemented!")
end
println("#TODO: Implement BFS via queue")


##  Utilities  ##

"Gets a node, or `nothing` if the handle is null."
@inline function try_deref_node(node_id, context = nothing)::Optional{Node}
    if is_null_id(node_id)
        return nothing
    else
        return deref_node(node_id, context)
    end
end

"Gets whether a node is somewhere inside the 'family' of another node."
is_deep_child_of(parent_id::ID, child_id::ID) where {ID} = (parent_id in parents(child_id, context))


export siblings, children, parents,
       family, family_breadth_first, family_breadth_first_deep
       try_deref_node
#


######################
#  Simple Iterators  #
######################

struct Siblings{TNodeID, TContext, F}
    root::Node{TNodeID, F}
    context::TContext
    include_root::Bool
end
struct Siblings_State{TNodeID, F}
    prev_idx::Int
    next_node::TNodeID
    idx_to_ignore::Int
end

# If the node has a parent, then we can access its cached child count.
@inline Base.haslength(s::Siblings) = !is_null_id(s.root.parent)
@inline Base.length(s::Siblings) = deref_node(s.root.parent, context).n_children

Base.eltype(::Siblings{TNodeID}) where {TNodeID} = TNodeID
@inline Base.iterate(s::Siblings) = iterate(s, start_sibling_iter(s))
@inline Base.iterate(::Siblings, ::Nothing) = nothing
@inline function Base.iterate( s::Siblings{TNodeID, TContext, F},
                               state::Siblings_State{TNodeID, F}
                             ) where {TNodeID, TContext, F}
    if is_null_id(state.next_node)
        return nothing
    else
        next_node = deref_node(state.next_node, s.context)
        new_state = Siblings_State(
            state.prev_idx + 1,
            next_node.sibling_next,
            state.idx_to_ignore
        )
        return (next_node, new_state)
    end
end
function start_sibling_iter( s::TSiblings,
                             offset::Int = 1  # Which index from here
                                              #    is the actual root of the search?
                           )::Optional{Siblings_State} where {TNodeID, TSiblings<:Siblings{TNodeID}}
    # Is the starting node the first sibling?
    if is_null_id(s.root.sibling_prev)
        # Start with the next sibling.
        # If there's no "next" sibling, then there aren't any siblings at all.
        if is_null_id(s.root.sibling_next)
            return nothing
        else
            idx_to_ignore = (s.include_root ? -1 : offset)
            return Siblings_State(0, s.root.sibling_next, idx_to_ignore)
        end
    # Otherwise, look backwards for the first sibling.
    else
        next_iter = Siblings(deref_node(s.root.sibling_prev, s.context),
                             s.context, s.include_root)
        return start_sibling_iter(next_iter, offset + 1)
    end
end


struct Children{TNodeID, TContext, F}
    parent::Node{TNodeID, F}
    context::TContext
end
@inline Base.length(c::Children) = c.parent.n_children
@inline Base.eltype(::Children{TNodeID}) where {TNodeID} = TNodeID
@inline Base.iterate(c::Children) = (is_null_id(c.parent.child_first) ?
                                         nothing :
                                         (c.parent.child_first, c.parent.child_first))
@inline function Base.iterate(c::Children, prev_id::TNodeID) where {TNodeID}
    prev_node = deref_node(prev_id, c.context)
    if is_null_id(prev_node.sibling_next)
        return nothing
    else
        return (prev_node.sibling_next, prev_node.sibling_next)
    end
end


struct Parents{TNodeID, TContext, F}
    start::Node{TNodeID, F}
    context::TContext
end
@inline Base.haslength(::Parents) = false
@inline Base.eltype(::Parents{TNodeID}) where {TNodeID} = TNodeID
@inline Base.iterate(p::Parents) = (is_null_id(p.start.parent) ?
                                        nothing :
                                        (p.start.parent, p.start.parent))
@inline function Base.iterate(p::Parents, prev_id::TNodeID) where {TNodeID}
    prev_node = deref_node(prev_id, p.context)
    if is_null_id(prev_node.parent)
        return nothing
    else
        return (prev_node.parent, prev_node.parent)
    end
end


##############################
#  "Whole family" iterators  #
##############################

println("#TODO: Depth-first iterator tracks its own depth and supports a min + max depth, to dramatically simplify and speed-up the BFS")

struct FamilyDFS{TNodeID, TContext}
    root_id::TNodeID
    context::TContext
    include_self::Bool
end
struct FamilyDFS_State{TNodeID}
    prev_id::TNodeID

    # Tracks the change in depth between the current element and the previous one.
    # Needed for FamilyBFS_NoHeap.
    previous_depth_delta::Int
end
@inline Base.haslength(::FamilyDFS) = false
@inline Base.eltype(::FamilyDFS{TNodeID}) where {TNodeID} = TNodeID
@inline function Base.iterate(f::FamilyDFS{TNodeID})::Optional{Tuple} where {TNodeID}
    if f.include_self
        return (f.root_id, FamilyDFS_State(f.root_id, 0))
    else
        node = deref_node(f.root_id, context)
        if is_null_id(node.child_first)
            return nothing
        else
            return (node.child_first, FamilyDFS_State(node.child_first, 0))
        end
    end
end
function Base.iterate( f::FamilyDFS{TNodeID},
                       state::FamilyDFS_State{TNodeID}
                     )::Optional{Tuple} where {TNodeID}
    prev_id = state.prev_id
    prev_node = deref_node(prev_id, f.context)

    depth_delta::Int = 0

    # If the current node has a child, keep going deeper.
    next_id::TNodeID = prev_node.child_first
    # If this node has no child, try a sibling instead.
    # Don't look at siblings/parents of the root node though,
    #    since that's outside the scope of the search.
    if is_null_id(next_id) && (prev_id != f.root_id)
        next_id = prev_node.sibling_next
        # If there's no next sibling, go up one level and try *that* parent's sibling.
        # Keep moving up until we can find an "uncle", or we hit the root node.
        while is_null_id(next_id) && (prev_node.parent != f.root_id)
            # Move up one level:
            prev_id = prev_node.parent
            prev_node = deref_node(prev_id, f.context)

            # Try to select the "uncle":
            next_id = prev_node.sibling_next
        end
    end

    return is_null_id(next_id) ?
               nothing :
               (next_id, FamilyDFS_State(next_id, depth_delta))
end


# The no-heap BFS implementation is based on Iterative-Deepening,
#    using successive applications of DFS.
# The downside is a lot of redundant visits to early nodes.
struct FamilyBFS_NoHeap{TNodeID, TContext}
    dfs::FamilyDFS{TNodeID, TContext}
end
struct FamilyBFS_NoHeap_State{TNodeID}
    dfs::FamilyDFS_State{TNodeID}
    current_target_depth::Int

    # As we iterate across a depth level, we need to remember
    #    whether the next depth level has any nodes.
    next_depth_any_nodes::Bool
end
@inline Base.haslength(::FamilyBFS_NoHeap) = false
@inline Base.eltype(::FamilyBFS_NoHeap{TNodeID}) where {TNodeID} = TNodeID
@inline function Base.iterate(f::FamilyBFS_NoHeap{TNodeID})::Optional{Tuple} where {TNodeID}
    first_iter::Optional{Tuple{TNodeID, FamilyDFS_State{TNodeID}}} =
        iterate(f.dfs)
    if isnothing(first_iter)
        return nothing
    else
        (first_element::TNodeID, dfs_state::FamilyDFS_State{TNodeID}) = first_iter
        initial_target_depth = (f.dfs.include_self ? 0 : 1)
        return (first_element, FamilyBFS_NoHeap_State(dfs_state, initial_target_depth, false))
    end
end
@inline function Base.iterate( f::FamilyBFS_NoHeap{TNodeID},
                               state::FamilyBFS_NoHeap_State{TNodeID}
                             )::Optional{Tuple} where {TNodeID}
    # Split out the fields of the iterator state,
    #    since it's immutable and we'll be using it a lot.
    (dfs_state, current_target_depth, next_depth_any_nodes) =
        (state.dfs, state.current_target_depth, state.next_depth_any_nodes)

    # Remember whether the node we just looked at has children.
    if !next_depth_any_nodes
        next_depth_any_nodes = (deref_node(dfs_state.prev_id, f.context).n_children > 0)
    end

    # Move the DFS forward until reaching another node at our current depth.
    dfs_depth::Int = current_target_depth
    local next_iter::Optional{Tuple{TNodeID, FamilyDFS_State{TNodeID}}}
    @do_while begin
        next_iter = iterate(f.dfs, dfs_state)
        # If the iterator finished, restart it with a deeper target depth.
        if isnothing(next_iter)
            # Are there any deeper nodes?
            # If so, restart the DFS search at the next depth level.
            if next_depth_any_nodes
                # We know we can skip the root node at this point, so make sure to do that
                #    to save a little time.
                inner_search = FamilyDFS(f.root_id, f.context, false)
                next_iter = iterate(inner_search)
                @bp_scene_tree_assert(exists(new_first_iter),
                                      "'next_depth_any_nodes' was true, yet there's nothing to iterate?????")

                # Update this iterator's state data.
                dfs_depth = 1
                next_depth_any_nodes = false
                current_target_depth += 1
            else
                return nothing
            end
        # The DFS didn't finish, so acknowledge it and continue.
        else
            dfs_depth += dfs_state.previous_depth_delta
            @bp_scene_tree_assert(dfs_depth >= 0,
                                  "An iteration of DFS within BFS just went above the root")
        end
    end (dfs_depth != current_target_depth)

    return isnothing(next_iter) ?
               nothing :
               (next_iter[1], FamilyBFS_NoHeap_State(dfs_state, current_target_depth, next_depth_any_nodes))
end


####################
#  Implementation  #
####################

"
Invalidates the cached world-space data.
May or may not include the rotation; for example if the node moved but didn't rotate,
    then there's no need to invalidate its world rotation.

Returns a copy of this node (does not update it in the context),
    but its children *will* be modified.
"
function invalidate_world_space( node::Node{TNodeID, F},
                                 context::TContext,
                                 include_rotation::Bool
                               )::Node{TNodeID, F} where {TNodeID, F, TContext}
    # Skip the work if caches are already invalidated.
    if !node.is_cached_world_mat && (!include_rotation || !node.cached_rot_world)
        # If this node doesn't have a cached world transform, then its children shouldn't either.
        @bp_scene_tree_debug begin
            for child_id::TNodeID in children(node, context)
                child::Node{TNodeID, F} = deref_node(child_id, context)
                @bp_check(!child.is_cached_world_mat,
                          "Child node has a cached world matrix while the direct parent doesn't")
                @bp_check(!include_rotation || !child.is_cached_world_rot,
                          "Child node has a cached world rotation while the direct parent doesn't")
            end
        end
    else
        # Invalidate this node's caches.
        @set! node.is_cached_world_mat = false
        if include_rotation
            @set! node.is_cached_world_rot = false
        end

        # Invalidate all childrens' caches.
        for child_id::TNodeID in children(node, context)
            updated_child = invalidate_world_space(
                deref_node(child_id, context),
                context,
                include_rotation
            )
            update_node(child_id, context, updated_child)
        end
    end

    return node
end

"
Removes the given node from under its parent.
Returns a copy of this node (does not update it in the context),
    but its parent and siblings *will* be modified.
This is part of the implementation for some other functions, and it has some quirks:
* It won't invalidate the node's cached world-space data like you'd expect.
"
function disconnect_parent( node::Node{TNodeID, F},
                            node_id::TNodeID,
                            context::TContext
                          )::Node{TNodeID, F} where {TNodeID, F, TContext}
    parent_data::Optional{Node{TNodeID, F}} = try_deref_node(node_id, context)

    # Update the parent.
    if exists(parent_data)
        @bp_scene_tree_assert(deref_node(node.parent, context) == parent_data,
                              "Passed in a node other than the parent!",
                              "\nGiven:\n\t", parent_data,
                              "\nActual parent:\n\t", deref_node(node.parent, context))
        if parent_data.child_first == node_id
            @bp_scene_tree_assert(is_null_id(node.sibling_prev),
                                  "I am my parent's child, but I have a previous sibling??")
            @set! parent_data.child_first = node.sibling_next
        end

        @set! parent_data.n_children -= 1

        update_node(node.parent, context, parent_data)
    end

    # Update the previous sibling in the tree.
    if !is_null_id(node.sibling_prev)
        sibling_data::Node{TNodeID, F} = deref_node(node.sibling_prev, context)
        @bp_scene_tree_assert(sibling_data.sibling_next == node_id,
                              "My 'previous' sibling has a different 'next' sibling; it isn't me")

        @set! sibling_data.sibling_next = node.sibling_next
        update_node(node.sibling_prev, context, sibling_data)
    end

    # Update the next sibling in the tree.
    if !is_null_id(node.sibling_next)
        sibling_data = deref_node(node.sibling_next, context)
        @bp_scene_tree_assert(sibling_data.sibling_prev == node_id,
                              "My 'next' sibling has a different 'previous' sibling; it isn't me")

        @set! sibling_data.sibling_prev = node.sibling_prev
        update_node(node.sibling_next, context, sibling_data)
    end
end