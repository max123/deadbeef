
/* driver that uses kmem allocator incorrectly to cause bugs */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include "kbad.h"

/*
 * The entire state of each kbad device.
 */
typedef struct {
	dev_info_t	*dip;		/* my devinfo handle */
} kbad_devstate_t;

/*
 * An opaque handle where our set of kbad devices lives
 */
static void *kbad_state;

static int kbad_open(dev_t *devp, int flag, int otyp, cred_t *cred);
static int kbad_read(dev_t dev, struct uio *uiop, cred_t *credp);
static int kbad_write(dev_t dev, struct uio *uiop, cred_t *credp);
static int kbad_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cred_p, int *rval_p);

static struct cb_ops kbad_cb_ops = {
	kbad_open,
	nulldev,	/* close */
	nodev,
	nodev,
	nodev,		/* dump */
	kbad_read,
	kbad_write,
	kbad_ioctl,		/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,
	NULL,
	D_NEW | D_MP
};

static int kbad_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result);
static int kbad_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int kbad_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

static struct dev_ops kbad_ops = {
	DEVO_REV,
	0,
	kbad_getinfo,
	nulldev,	/* identify */
	nulldev,	/* probe */
	kbad_attach,
	kbad_detach,
	nodev,		/* reset */
	&kbad_cb_ops,
	(struct bus_ops *)0
};


extern struct mod_ops mod_driverops;

static struct modldrv modldrv = {
	&mod_driverops,
	"kbad driver v1.0",
	&kbad_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	0
};

int
_init(void)
{
	int e;

	if ((e = ddi_soft_state_init(&kbad_state,
	    sizeof (kbad_devstate_t), 1)) != 0) {
		return (e);
	}

	if ((e = mod_install(&modlinkage)) != 0)  {
		ddi_soft_state_fini(&kbad_state);
	}

	return (e);
}

int
_fini(void)
{
	int e;

	if ((e = mod_remove(&modlinkage)) != 0)  {
		return (e);
	}
	ddi_soft_state_fini(&kbad_state);
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
kbad_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int instance;
	kbad_devstate_t *rsp;

	switch (cmd) {

	case DDI_ATTACH:

		instance = ddi_get_instance(dip);

		if (ddi_soft_state_zalloc(kbad_state, instance) != DDI_SUCCESS) {
			cmn_err(CE_CONT, "%s%d: can't allocate state\n",
			    ddi_get_name(dip), instance);
			return (DDI_FAILURE);
		} else
			rsp = ddi_get_soft_state(kbad_state, instance);

		if (ddi_create_minor_node(dip, "kbad", S_IFCHR,
		    instance, DDI_PSEUDO, 0) == DDI_FAILURE) {
			ddi_remove_minor_node(dip, NULL);
			goto attach_failed;
		}

		rsp->dip = dip;
		ddi_report_dev(dip);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}

attach_failed:
	(void) kbad_detach(dip, DDI_DETACH);
	return (DDI_FAILURE);
}

static int
kbad_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int instance;
	register kbad_devstate_t *rsp;

	switch (cmd) {

	case DDI_DETACH:
		ddi_prop_remove_all(dip);
		instance = ddi_get_instance(dip);
		rsp = ddi_get_soft_state(kbad_state, instance);
		ddi_remove_minor_node(dip, NULL);
		ddi_soft_state_free(kbad_state, instance);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
kbad_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	kbad_devstate_t *rsp;
	int error = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((rsp = ddi_get_soft_state(kbad_state,
		    getminor((dev_t)arg))) != NULL) {
			*result = rsp->dip;
			error = DDI_SUCCESS;
		} else
			*result = NULL;
		break;

	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;

	default:
		break;
	}

	return (error);
}


#define KBADSIZE 256 /* dtrace shows more 256 byte allocs than others */
#define DELAYTIME (60 * drv_usectohz(1000000))

/* cause deadbeef panic */

static void
kbad_timeout_handler(void *arg)
{
	int i;
	char *p = (char *)arg;
	for (i = 0; i < KBADSIZE; i++)
		*p++ = '?';
}


void
kbad_bad(void)
{
	char *p;
	int nticks;

	p = kmem_alloc(KBADSIZE, KM_SLEEP);
	if (!p) {
		cmn_err(CE_WARN, "kbad_bad: out of memory\n");
		return;
	}
	kmem_free(p, KBADSIZE);

	(void) timeout(kbad_timeout_handler, p, DELAYTIME);

	return;
}


/*ARGSUSED*/
static int
kbad_open(dev_t *devp, int flag, int otyp, cred_t *cred)
{
	if (otyp != OTYP_BLK && otyp != OTYP_CHR)
		return (EINVAL);

	if (ddi_get_soft_state(kbad_state, getminor(*devp)) == NULL)
		return (ENXIO);

	return (0);
}


/*ARGSUSED*/
static int
kbad_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
	int instance = getminor(dev);
	kbad_devstate_t *rsp = ddi_get_soft_state(kbad_state, instance);
	return(0);

}

/*ARGSUSED*/
static int
kbad_write(dev_t dev, register struct uio *uiop, cred_t *credp)
{
	int instance = getminor(dev);
	kbad_devstate_t *rsp = ddi_get_soft_state(kbad_state, instance);
	return(0);
}

/*ARGSUSED*/
static int
kbad_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cred_p, int *rval_p)
{
    switch(cmd) {
    case BAD_KBAD:
	    kbad_bad();
	    return 0;
    default:
	    return (EINVAL);
    }
}
