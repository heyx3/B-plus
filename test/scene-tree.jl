# Create a simple test scene-tree backed by a set of reference-types.
#DEBUG: Disable the test
if false
######################
#  Data Definitions  #
######################

mutable struct ST_Entity
    transform::Node{Int, Float32}
    is_root::Bool
end
const ST_World = Set{ST_Entity}

const ST_NodeID = Optional{ST_Entity}
const ST_Node = SceneTree.Node{Int, Float32}


##############################
#  Interface Implementation  #
##############################

SceneTree.deref_node(id::ST_Entity) = id.transform
SceneTree.null_node_id(::Type{ST_NodeID}) = nothing
SceneTree.update_node(id::ST_Entity, data::ST_Node) = (id.transform = data)

SceneTree.on_rooted(id::ST_Entity) = (id.is_root = true)
SceneTree.on_uprooted(id::ST_Entity) = (id.is_root = false)


######################
#  Scene Definition  #
######################

# Create the nodes. They will be assigned parents later.
const SCENE_TREE = map(node -> ST_Entity(node, true), [
    # The root nodes:
    ST_Node(v3f(1, 0, 0)),
    ST_Node(v3f(2, 0, 0)),
    ST_Node(v3f(3, 0, 0)),
    ST_Node(v3f(4, 0, 0)),

    # The direct children:
    ST_Node(v3f(1, 1, 0)), # 5
    ST_Node(v3f(1, 2, 0)),
    ST_Node(v3f(1, 3, 0)),
    ST_Node(v3f(2, 1, 0)),
    ST_Node(v3f(3, 1, 0)),
    ST_Node(v3f(3, 2, 0)), # 10

    # Some grand-children:
    ST_Node(v3f(1, 1, 1)), # 11
    ST_Node(v3f(1, 3, 1)),
    ST_Node(v3f(1, 3, 2)),
    ST_Node(v3f(3, 1, 1)),
    ST_Node(v3f(3, 1, 2)), # 15
    ST_Node(v3f(3, 1, 3)),
    ST_Node(v3f(3, 1, 4)),
    ST_Node(v3f(3, 1, 5)),
    ST_Node(v3f(3, 1, 6))  # 19
])


# Set up the hierarchical relationships.
# Keep in mind that new children are inserted at the front of the parent's list.
function test_set_parent(child::Int, parent::Int, context = SCENE_TREE)
    @bp_test_no_allocations(
        begin
            SceneTree.set_parent(SCENE_TREE[child], SCENE_TREE[parent], context)
            (SCENE_TREE[child].parent, SCENE_TREE[parent].child_first)
        end,
        (SCENE_TREE[parent], SCENE_TREE[child])
    )
end
test_set_parent(7, 1)
test_set_parent(6, 1)
test_set_parent(5, 1)
test_set_parent(8, 2)
test_set_parent(10, 3)
test_set_parent(9, 3)
test_set_parent(11, 5)
test_set_parent(13, 7)
test_set_parent(12, 7)
test_set_parent(19, 9)
test_set_parent(18, 9)
test_set_parent(17, 9)
test_set_parent(16, 9)
test_set_parent(15, 9)
test_set_parent(14, 9)


###########
#  Tests  #
###########

println("#TODO: Add SceneTree unit tests")

end # if fase