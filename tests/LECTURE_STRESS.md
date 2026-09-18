# Long-lecture stress test

Build and run from the repository root:

```sh
mkdir -p tests/lecture-stress-build
cd tests/lecture-stress-build
qmake6 ../lecture_stress.pro
make -j"$(nproc)"
QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= \
  ./lecture_stress ../../src/Main.qml
```

The test creates a 1,500-row note and times direct QML operations. It queues
renders but does not wait for LaTeX jobs to finish.
