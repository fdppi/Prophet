ABOUT PROPHET
====

Prophet is a parallel instruction-oriented simulation framework for many-cores. Prophet adopts a general instruction-oriented model to simulate processor cores, in which a simulator is built from the perspective of each simulated instruction impacting a small number of relevant processor components.

We have designed and implemented a prototype of Prophet that supports both user-level and full-system simulation with COREMU and Intel Pin. We also implemented a version that can run on Intel Xeon Phi.


BUILD & RUN
====

Full-system with Coremu
----
Install Coremu according to https://sourceforge.net/p/coremu/home/Home/. Prophet compiles with g++ version 4.8.0 or higher. To compile Prophet, run "make" under "calculation". You can now run Prophet with the following command under "coremu": 
cd scripts
sudo ./x86_64-linux.sh <path to coremu> <path to image> <number of emulated cores>

User-level with Pin
----
Prophet compiles with g++ version 4.8.0 or higher. To compile Prophet, run "make" under "pin_opal".
You also need Intel Pin 2.12 or higher. 
After that run Intel Pin with the following command:
${PIN HOME}/pin -t ${your pin_opal}/obj-intel64/pin_opal.so -- ${your benchmark}

User-level on MIC
----
Intel MPSS is needed to compile Prophet for MIC. Then, run make under "calculation.server" and "pin_opal.host". Copy cal_server under "calculation.server" to the MIC node.
After that, run cal_server under each node to start listening. Then run the same command at the host:
${PIN HOME}/pin -t ${your pin_opal}/obj-intel64/pin_opal.so -- ${your benchmark}
