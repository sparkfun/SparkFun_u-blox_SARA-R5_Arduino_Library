SparkFun u-blox SARA-R5 Arduino Library
==============================

<table class="table table-hover table-striped table-bordered">
    <tr align="center">
      <td><a href="https://www.sparkfun.com/products/17272"><img src="https://cdn.sparkfun.com/assets/parts/1/6/2/7/9/17272-SparkFun_MicroMod_Asset_Tracker_Carrier_Board-01a.jpg" alt="MicroMod Asset Tracker Carrier Board"></a></td>
      <td><a href="https://www.sparkfun.com/products/18031"><img src="https://cdn.sparkfun.com/assets/parts/1/7/2/6/0/18031-SparkFun_LTE_GNSS_Breakout_-_SARA-R5-01.jpg" alt="SparkFun LTE GNSS Breakout - SARA-R5"</a></td>
    </tr>
    <tr align="center">
      <td><a href="https://www.sparkfun.com/products/17272">MicroMod Asset Tracker Carrier Board</a></td>
      <td><a href="https://www.sparkfun.com/products/18031">LTE GNSS Breakout - SARA-R5</a></td>
    </tr>
</table>
       
An Arduino library for the u-blox SARA-R5 LTE-M / NB-IoT modules with secure cloud, as used on the [SparkFun MicroMod Asset Tracker](https://www.sparkfun.com/products/17272) and the [SparkFun LTE GNSS Breakout - SARA-R5](https://www.sparkfun.com/products/18031).

v1.1 has had a thorough update and includes new features and examples. This library now supports up to 7 simultaneous TCP or UDP sockets. There are new examples to show how to play ping pong with multiple TCP and UDP sockets.

v1.1 also supports binary data transfers correctly. There are new examples showing how you can integrate this library with the [SparkFun u-blox GNSS Arduino Library](https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library) and use the SARA-R5 to: download AssistNow Online and Offline data and push it to the GNSS; open a connection to a NTRIP Caster (such as RTK2go, Skylark or Emlid Caster) and push RTK correction data to the GNSS.

You can install this library using the Arduino IDE Library Manager: search for _**SparkFun u-blox SARA-R5**_

## Repository Contents

* **/examples** - Example sketches for the library (.ino). Run these from the Arduino IDE.
* **/src** - Source files for the library (.cpp, .h).
* **keywords.txt** - Keywords from this library that will be highlighted in the Arduino IDE.
* **library.properties** - General library properties for the Arduino package manager.

## Contributing

If you would like to contribute to this library: please do, we truly appreciate it, but please follow [these guidelines](./CONTRIBUTING.md). Thanks!

## Documentation

* **[Installing an Arduino Library Guide](https://learn.sparkfun.com/tutorials/installing-an-arduino-library)** - Basic information on how to install an Arduino library.
* **[MicroMod Asset Tracker Hookup Guide](https://learn.sparkfun.com/tutorials/micromod-asset-tracker-carrier-board-hookup-guide)** - Hookup Guide for the MicroMod Asset Tracker Carrier Board.
* **[MicroMod Asset Tracker Product Repository](https://github.com/sparkfun/MicroMod_Asset_Tracker)** - MicroMod Asset Tracker repository (including hardware files).
* **[SparkFun LTE GNSS Breakout - SARA-R5 Hookup Guide](https://learn.sparkfun.com/tutorials/lte-gnss-breakout---sara-r5-hookup-guide)** - Hookup Guide for the LTE GNSS Breakout - SARA-R5.
* **[SparkFun LTE GNSS Breakout - SARA-R5 Product Repository](https://github.com/sparkfun/SparkFun_LTE_GNSS_Breakout_SARA-R510M8S)** - LTE GNSS Breakout - SARA-R5 repository (including hardware files).
* **[LICENSE.md](./LICENSE.md)** - License Information

## Products that use this library

* **[DEV-17272](https://www.sparkfun.com/products/17272)** - MicroMod Asset Tracker
* **[GPS-18031](https://www.sparkfun.com/products/18031)** - SparkFun LTE GNSS Breakout - SARA-R5

- Your friends at SparkFun.
