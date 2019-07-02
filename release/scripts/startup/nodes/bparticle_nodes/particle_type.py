import bpy
from .. base import BParticlesNode
from .. socket_builder import SocketBuilder

class ParticleTypeNode(bpy.types.Node, BParticlesNode):
    bl_idname = "bp_ParticleTypeNode"
    bl_label = "Particle Type"

    def declaration(self, builder : SocketBuilder):
        builder.emitter_input("emitters", "Emitters")
