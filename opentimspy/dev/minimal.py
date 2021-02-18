# There is an MS1 at 45613 and 45654, but there is no MS1 frame between 45633 and 45634 :-)
from opentimspy.opentims import OpenTIMS

folder = '/home/matteo/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d'

D = OpenTIMS(folder)
D.min_mz
D.max_mz
D.min_inv_ion_mobility
D.max_inv_ion_mobility
D.min_retention_time
D.max_retention_time
D.frames
D.GlobalMetadata