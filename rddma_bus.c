#include <linux/rddma.h>

#include <linux/rddma_bus.h>
#include <linux/device.h>

ssize_t rddma_bus_attr_fred_show(struct bus_type *bus, char *buf)
{
	return sprintf(buf, "Hi I'm fred of %s\n",bus->name);
}

ssize_t rddma_bus_attr_fred_store(struct bus_type *bus, const char *buf, size_t count)
{
	return count;
}

struct bus_attribute rddma_bus_attribute = {
	.attr = {"rddma_bus_attr_fred", THIS_MODULE, 0644},
	.show = rddma_bus_attr_fred_show,
	.store = rddma_bus_attr_fred_store,
};

int rddma_bus_match(struct device *dev, struct device_driver *drv)
{
	return 0; /* Not mine */
}

int rddma_bus_probe(struct device *dev)
{
	return 0;
}

int rddma_bus_uevent(struct device *dev, char **envp, int num_envp, char *buffer, int buffer_size)
{
	return -ENODEV;
}

struct bus_type rddma_bus_type = {
	.name = "rddma_bus",
	.probe = rddma_bus_probe,
	.match = rddma_bus_match,
 	.uevent = rddma_bus_uevent, 
};

int rddma_bus_register(struct rddma_subsys *rsys)
{
	return bus_register(&rddma_bus_type);
}

void rddma_bus_unregister(struct rddma_subsys *rsys)
{
	bus_unregister(&rddma_bus_type);
}
