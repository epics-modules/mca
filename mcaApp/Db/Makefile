TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install)
# databases, templates, substitutions like this
DB += 13element.db
DB += 13element_sum.db
DB += 16element.db
DB += 3element.db
DB += RontecXFlash.db
DB += SIS38XX.template
DB += SIS38XX_waveform.template
DB += icbDsp.db
DB += icb_adc.db
DB += icb_amp.db
DB += icb_hvps.db
DB += icb_tca.db
DB += mca.db
DB += mcaReadNotify.db
DB += mcaStdRecords.template
DB += mcaSum13.db
DB += mcaSum8.db
DB += simple_mca.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

