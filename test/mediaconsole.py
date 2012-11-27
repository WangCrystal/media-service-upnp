# mediaconsole
#
# Copyright (C) 2012 Intel Corporation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
# for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#
# Mark Ryan <mark.d.ryan@intel.com>
#

import dbus
import sys
import json

def print_properties(props):
    print json.dumps(props, indent=4, sort_keys=True)

class MediaObject(object):

    def __init__(self, path):
        bus = dbus.SessionBus()
        self.__propsIF = dbus.Interface(bus.get_object(
                'com.intel.media-service-upnp', path),
                                        'org.freedesktop.DBus.Properties')
        self.__objIF = dbus.Interface(bus.get_object(
                'com.intel.media-service-upnp', path),
                                      'org.gnome.UPnP.MediaObject2')

    def get_props(self, iface = ""):
        return self.__propsIF.GetAll(iface)

    def get_prop(self, prop_name, iface = ""):
        return self.__propsIF.Get(iface, prop_name)

    def print_prop(self, prop_name, iface = ""):
        print_properties(self.__propsIF.Get(iface, prop_name))

    def print_props(self, iface = ""):
        print_properties(self.__propsIF.GetAll(iface))

    def print_dms_id(self):
        path = self.__propsIF.Get("", "Path")
        dms_id = path[path.rfind("/") + 1:]
        i = 0
        while i+1 < len(dms_id):
            num = dms_id[i] + dms_id[i+1]
            sys.stdout.write(unichr(int(num, 16)))
            i = i + 2
        print

    def delete(self):
        return self.__objIF.Delete()

    def update(self, to_add_update, to_delete):
        return self.__objIF.Update(to_add_update, to_delete)

class Item(MediaObject):
    def __init__(self, path):
        MediaObject.__init__(self, path)
        bus = dbus.SessionBus()
        self.__itemIF = dbus.Interface(bus.get_object(
                'com.intel.media-service-upnp', path),
                                       'org.gnome.UPnP.MediaItem2')

    def print_compatible_resource(self, protocol_info, fltr):
        print_properties(self.__itemIF.GetCompatibleResource(protocol_info,
                                                             fltr))

class Container(MediaObject):

    def __init__(self, path):
        MediaObject.__init__(self, path)
        bus = dbus.SessionBus()
        self.__containerIF = dbus.Interface(bus.get_object(
                'com.intel.media-service-upnp', path),
                                            'org.gnome.UPnP.MediaContainer2')

    def list_children(self, offset, count, fltr, sort=""):
        objects = self.__containerIF.ListChildrenEx(offset, count, fltr, sort)
        for item in objects:
            print_properties(item)
            print ""

    def list_containers(self, offset, count, fltr, sort=""):
        objects = self.__containerIF.ListContainersEx(offset, count, fltr, sort)
        for item in objects:
            print_properties(item)
            print ""

    def list_items(self, offset, count, fltr, sort=""):
        objects = self.__containerIF.ListItemsEx(offset, count, fltr, sort)
        for item in objects:
            print_properties(item)
            print ""

    def search(self, query, offset, count, fltr, sort=""):
        objects, total = self.__containerIF.SearchObjectsEx(query, offset,
                                                            count, fltr, sort)
        print "Total Items: " + str(total)
        print
        for item in objects:
            print_properties(item)
            print ""

    def tree(self, level=0):
        objects = self.__containerIF.ListChildren(
            0, 0, ["DisplayName", "Path", "Type"])
        for props in objects:
            print (" " * (level * 4) + props["DisplayName"] +
                   " : (" + props["Path"]+ ")")
            if props["Type"] == "container":
                Container(props["Path"]).tree(level + 1)

    def upload(self, name, file_path):
        (tid, path) = self.__containerIF.Upload(name, file_path)
        print "Transfer ID: " + str(tid)
        print "Path: " + path

    def create_container(self, name, type, child_types):
        path = self.__containerIF.CreateContainer(name, type, child_types)
        print "New container path: " + path

class Device(Container):

    def __init__(self, path):
        Container.__init__(self, path)
        bus = dbus.SessionBus()
        self.__deviceIF = dbus.Interface(bus.get_object(
                'com.intel.media-service-upnp', path),
                                         'com.intel.UPnP.MediaDevice')

    def upload_to_any(self, name, file_path):
        (tid, path) = self.__deviceIF.UploadToAnyContainer(name, file_path)
        print "Transfer ID: " + str(tid)
        print "Path: " + path

    def create_container_in_any(self, name, type, child_types):
        path = self.__deviceIF.CreateContainerInAnyContainer(name, type,
                                                             child_types)
        print "New container path: " + path

    def get_upload_status(self, id):
        (status, length, total) = self.__deviceIF.GetUploadStatus(id)
        print "Status: " + status
        print "Length: " + str(length)
        print "Total: " + str(total)

    def get_upload_ids(self):
        upload_ids  = self.__deviceIF.GetUploadIDs()
        print_properties(upload_ids)

    def cancel_upload(self, id):
        self.__deviceIF.CancelUpload(id)

    def cancel(self):
        return self.__deviceIF.Cancel()

class UPNP(object):

    def __init__(self):
        bus = dbus.SessionBus()
        self.__manager = dbus.Interface(bus.get_object(
                'com.intel.media-service-upnp',
                '/com/intel/MediaServiceUPnP'),
                                        'com.intel.MediaServiceUPnP.Manager')

    def servers(self):
        for i in self.__manager.GetServers():
            try:
                server = Container(i)
                try:
                    folderName = server.get_prop("FriendlyName");
                except Exception:
                    folderName = server.get_prop("DisplayName");
                print u'{0:<30}{1:<30}'.format(folderName , i)
            except dbus.exceptions.DBusException, err:
                print u"Cannot retrieve properties for " + i
                print str(err).strip()[:-1]

    def version(self):
        print self.__manager.GetVersion()

    def set_protocol_info(self, protocol_info):
        self.__manager.SetProtocolInfo(protocol_info)

    def prefer_local_addresses(self, prefer):
        self.__manager.PreferLocalAddresses(prefer)
