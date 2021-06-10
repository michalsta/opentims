ThisBuild / organization := "org.davidteschner"

ThisBuild / version := "0.1-SNAPSHOT"

name := "timsj"

publishTo := Some(Resolver.file("local-ivy", file("$HOME/.ivy2/local/")))

scalaVersion := "2.12.12"

libraryDependencies ++= Seq(
  "com.almworks.sqlite4java" % "sqlite4java" % "1.0.392"
)

run / mainClass := Some("Example")
