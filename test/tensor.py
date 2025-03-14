import opentimspy

O = opentimspy.OpenTIMS("/home/midia_rawdata/G_8027.d")
X = O.frames_as_tensor([1,4,5])
print(X)
print(X.shape)