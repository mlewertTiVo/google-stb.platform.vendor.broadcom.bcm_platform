/** \file containers_documentation.h
 * Documentation for the Containers Framework
 */

/** \mainpage Multimedia Containers API
 *
 * \section intro_sec Introduction
 * A multimedia container format is a format whose specification describes how multimedia data and
 * metadata are stored (see http://en.wikipedia.org/wiki/Container_format_(digital)).
 *
 * The aim of the API described in this document is to provide a generic and relatively low-level
 * API that is used to drive different implementations of container readers and writers.
 *
 * The public API for this is described here: \ref VcContainerApi
 *
 * \section features_sec Features
 * The multimedia containers API has been designed to support all the following features:
 * - Portability (the API is self-contained)
 * - Reentrancy (multiple instances of readers and writers can run concurrently)
 * - Extensible without breaking source or binary backward compatibility
 * - Abstracted Input and Output (container readers and writers are independant of the actual I/O implementation)
 * - One point of entry for applications (applications do not need to know about the individual readers and writers)
 * - Provides a list of capabilities that readers and writers use to let the application know what features they support
 * - Support for multiple audio and video tracks
 * - Support for multiple concurrent programs (e.g. MPEG TS)
 * - Support for chapters (e.g. DVD or user defined chapters)
 * - Support for progress reporting (especially useful for operations which can take a long time. e.g. opening or seeking)
 *
 * \section diagram_sec Overview Diagram
 *
 * \dot
digraph G
{
bgcolor="transparent";
node [fontname="FreeSans",fontsize="10",shape=record];

NodeC [label="Client"];
Node0 [label="{<p0> Container API| <p1> Container Common Code Layer}", URL="$group__VcContainerApi.html"];
Node1 [label="{<p0> Container Module API| <p1> Container Reader 1}", URL="$group__VcContainerModuleApi.html"];
Node2 [label="{<p0> Container Module API| <p1> Container Reader 2}", URL="$group__VcContainerModuleApi.html"];
Node3 [label="{<p0> Container Module API| <p1> Container Writer 1}", URL="$group__VcContainerModuleApi.html"];
Node4 [label="{<p0> Container Module API| <p1> Container Writer 2}", URL="$group__VcContainerModuleApi.html"];
Node5 [label="{<p0> Container I/O API| <p1> Container I/O Common Code Layer}", URL="$group__VcContainerModuleApi.html"];
Node6 [label="{<p0> Container I/O Module API| <p1> Container I/O reader 1}", URL="$group__VcContainerModuleApi.html"];
Node7 [label="{<p0> Container I/O Module API| <p1> Container I/O reader 2}", URL="$group__VcContainerModuleApi.html"];
Node8 [label="{<p0> Container I/O Module API| <p1> Container I/O Writer 1}", URL="$group__VcContainerModuleApi.html"];
Node9 [label="{<p0> Container I/O Module API| <p1> Container I/O Writer 2}", URL="$group__VcContainerModuleApi.html"];

NodeC->Node0;
Node0->Node1;
Node0->Node2;
Node0->Node3;
Node0->Node4;
Node1->Node5;
Node2->Node5;
Node3->Node5;
Node4->Node5;
Node5->Node6;
Node5->Node7;
Node5->Node8;
Node5->Node9;
}
 \enddot
 *
 * \section container_api_sec Container API
 * The \ref VcContainerApi is the public API for using containers. It provides a single point of entry
 * for using containers, which make it easy to use for applications since they don't have to know anything
 * about the specific container they are trying to read. This also provides a place to implement slightly
 * higher level common code. Things like re-packetizing or indexing could be done at this level by
 * calling into separate libraries.
 *
 * \section container_module_api_sec Container Module API
 * The \ref VcContainerModuleApi is the private API for implementing container readers and writers.
 * It is aimed to be a slightly lower level API than the \ref VcContainerApi. Basically container readers
 * and writers should try to implement only thoses features which are supported by the underlying format
 * and advertise their (restricted) capabilities. Any clever things can then be implemented in a generic
 * and re-usable way by the layer of code just below the Container API.
 *
 * \section io_sec I/O Abstraction
 * All the input / output operations are abstracted using the \ref VcContainerIoApi.
 * The aim is to provide container i/o modules for the most common i/o types (file, http, etc) while
 * still allowing customers to add their own i/o modules should they wish so.
 * The I/O abstraction layer also provides an extended set of helper functions to make things easier
 * for the containers (e.g. reading integers of different width and endianess).
 *
 */
