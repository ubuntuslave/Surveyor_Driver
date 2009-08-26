SRC = surveyor_driver.cc surveyor_driver.h surveyor_comms.c surveyor_comms.h
OBJLIBS = libsurveyordriver.so
OBJS = surveyor_driver.o surveyor_comms.o

all: $(OBJLIBS)

$(OBJS): $(SRC)
	echo "Building the SurveyorDriver plugin..."
	$(CXX) -Wall -fpic -g3 `pkg-config --cflags playercore` -c $(SRC)

$(OBJLIBS): $(OBJS)
	$(CXX) -shared -nostartfiles -o $@ $^

clean:
	echo "Cleaning up the SurveyorDriver plugin..."
	rm -f $(OBJS) $(OBJLIBS) *.gch
