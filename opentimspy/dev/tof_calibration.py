"""
except for HPC calibration which uses a polynome to improve accuracy in the low m/z region we use the following
transformation formula:

time = C0 + sqrt(1.0e12/C1)*sqrt(m+dm) + c2*(m+dm) + c3*(m+dm)^1.5

In addition we use temperature correction for coefficients C1 and C2.
Reference temperatures and correction factors are store in the table MzCalibration:
T1_ref, T2_ref, dC1, dC2

Actual temperature values are read from the frames table for each frame, columns T1 and T1

The correction formula is
alpha = dC1*(T1_ref - T1) + dC2*(T2_ref - T2)
beta = 1 + alpha/1.0e6

C1_corrected = C1*beta
C2_corrected = C2/beta

"""

from bdal.io.timsdata import TimsData
import os, sys, math
import scipy.optimize as sci

if len(sys.argv) < 2:
    raise RuntimeError("need arguments: tdf_directory")

analysis_dir = sys.argv[1]


tims_file = TimsData(analysis_dir)
frameId=1
mz1 = 599.31
t2_index = 131941;

c = tims_file.conn

# temperature compensation
def get_temperature_correction(conn, frame_id):
    mz_calib_id = c.execute("select MzCalibration from Frames where Id={}".format(frame_id)).fetchone()[0]
    T1_ref = c.execute("select T1 from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]
    T2_ref = c.execute("select T2 from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]
    factor1 = c.execute("select dC1 from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]
    factor2 = c.execute("select dC2 from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]

    T1_frame = c.execute("select T1 from Frames where Id={}".format(frame_id)).fetchone()[0]
    T2_frame = c.execute("select T2 from Frames where Id={}".format(frame_id)).fetchone()[0]

    alpha = factor1*(T1_ref-T1_frame) + factor2*(T2_ref-T2_frame)
    return 1 + alpha/1.0e6

def get_tof_transformation(conn, temp_corr, frame_id):
    # type one calibration, values taken from tdf-sdk example data
    mz_calib_id = c.execute("select MzCalibration from Frames where Id={}".format(frame_id)).fetchone()[0]
    c0 = c.execute("select C0 from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]
    c1 = c.execute("select C1 from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]
    c2 = c.execute("select C2 from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]
    c3 = c.execute("select C3 from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]
    c4 = c.execute("select C4 from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]

    digitizer_base = c.execute("select DigitizerTimebase from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]
    digitizer_delay = c.execute("select DigitizerDelay from MzCalibration where Id={}".format(mz_calib_id)).fetchone()[0]
    # temperature compensation
    c1 = c1*temp_corr
    c2 = c2/temp_corr

    def tof_index_transformation(mz):
        mz_corr = mz + c4
        # transformation formula mz => tof
        tof = c0 + math.sqrt(1.0e12/c1 * mz_corr) + c2*mz_corr + c3*math.pow(mz_corr, 1.5)
        # simple linear transformation tof => tof_index: tof_time = digitizer_base*index + digitizer_delay
        return (tof - digitizer_delay) / digitizer_base

    return tof_index_transformation

temp_corr = get_temperature_correction(c, frameId)
print ("temperature correction factor is {}".format(temp_corr))

tof_index_trafo = get_tof_transformation(c, temp_corr, frameId)

t1_index = tof_index_trafo(mz1)
print ("time of flight index for {} is {}".format(mz1, t1_index))

# now calculate mz for that index using the sdk function
mz11 = tims_file.indexToMz(frameId, [t1_index])
print ("mz value with sdk calculated is {}".format(mz11[0]))

# in order to have a transformation function tof-index => mz we use Newton's method
def mz_transformation(index, frame_id):
    tof_index_trafo = get_tof_transformation(c, temp_corr, frameId)

    def tof_index_polynome(mz):
        return tof_index_trafo(mz) - index

    return sci.newton(tof_index_polynome, 1, tol=1.0e-2)

# now we have a transformation function tof_index => m/z for a specific frame
mz2 = mz_transformation(t2_index, frameId)

print ("newton method: mz for index={}: {}".format(t2_index, mz2))

# now again calculate mz for that index using the sdk function
mz3 = tims_file.indexToMz(frameId, [t2_index])
print ("mz value with sdk calculated is {}".format(mz3[0]))


