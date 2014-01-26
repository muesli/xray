TEMPLATE = app

QT += network xml
LIBS += -lpHash

#HEADERS =
SOURCES = src/main.cpp

isEmpty(PREFIX) {
  PREFIX=/usr/local
}
TARGET = xray

target.path = $$PREFIX/bin
INSTALLS += target
