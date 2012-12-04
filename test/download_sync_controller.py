# download_sync_controller
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
# Regis Merlino <regis.merlino@intel.com>
#

import ConfigParser
import os
import dbus

from mediaconsole import UPNP, Container, Device

class _DscUpnp(UPNP):
    def __init__(self):
        UPNP.__init__(self)

    def get_servers(self):
        return self._manager.GetServers()

class _DscContainer(Container):
    def __init__(self, path):
        Container.__init__(self, path)
        self.__path = path

    def find_containers(self):
            return self._containerIF.SearchObjectsEx(
                                        'Type derivedfrom "container"',
                                        0, 0,
                                        ['DisplayName', 'Path', 'Type'], '')[0]
    def find_updates(self, upd_id):
            return self._containerIF.SearchObjectsEx(
                            'ObjectUpdateID > "{0}"'.format(upd_id),
                            0, 0,
                            ['DisplayName', 'Path', 'Type', 'Parent'], '')[0]

    def find_children(self):
        return self._containerIF.ListChildrenEx(0, 0,
                                             ['DisplayName', 'Path', 'Parent',
                                              'Type'], '')

class DscError(Exception):
    def __init__(self, str):
        self.str = str

    def __str__(self):
        return 'DscError: ' + self.str

class _DscStore(object):
    ITEM_NEW = 1
    ITEM_UPDATE = 2
    CONTAINER_NEW = 3

    def __init__(self, root_path, server_id):
        if not os.path.exists(root_path):
            os.makedirs(root_path)

        self.__config_path = root_path + '/' + server_id + '.conf'
        self.__config = ConfigParser.ConfigParser()
        self.__config.read(self.__config_path)

    def __write_config(self):
        with open(self.__config_path, 'wb') as configfile:
            self.__config.write(configfile)

    def __id_from_path(self, path):
        return os.path.basename(path)

    def __orig_id(self, media_object):
        try:
            return self.__id_from_path(media_object['RefPath'])
        except KeyError:
            return self.__id_from_path(media_object['Path'])

    def __removed_items(self, local_ids, remote_items):
        for local_id in local_ids:
            found = False

            for remote in remote_items:
                remote_id = self.__orig_id(remote)
                if local_id == remote_id:
                    found = True

            if not found:
                yield local_id

    def __sync_item(self, obj, obj_id, parent_id, status, write_conf):
        if status == _DscStore.ITEM_UPDATE:
            print u'\tMedia "{0}" updated'.format(obj['DisplayName'])
        elif status == _DscStore.ITEM_NEW:
            print u'\tNew media "{0}" tracked'.format(obj['DisplayName'])
        else:
            pass

        self.__config.set(parent_id, obj_id, obj['DisplayName'])
        
        if write_conf:
            self.__write_config()

    def reset(self):
        if os.path.exists(self.__config_path):
            os.remove(self.__config_path)

        self.__config = ConfigParser.ConfigParser()

    def sync_container(self, container, items):
        print u'Syncing container  {0}...'.format(container['DisplayName'])

        container_id = self.__id_from_path(container['Path'])
        if not self.__config.has_section(container_id):
            self.__config.add_section(container_id)

        for remote in items:
            remote_id = self.__id_from_path(remote['Path'])
            if not self.__config.has_option(container_id, remote_id):
                if remote['Type'] == 'container':
                    status = _DscStore.CONTAINER_NEW
                else:
                    status = _DscStore.ITEM_NEW
                self.__sync_item(remote, remote_id, container_id, status, False)

        for local in self.__removed_items(
                                    self.__config.options(container_id), items):
            if self.__config.has_section(local):
                print u'\tRemoved container "{0}"' \
                                .format(self.__config.get(container_id, local))
                self.__config.remove_option(container_id, local)
                self.__config.remove_section(local)
            else:
                print u'\tRemoved media "{0}"' \
                                .format(self.__config.get(container_id, local))
                self.__config.remove_option(container_id, local)

        self.__write_config()

    def sync_item(self, obj):
        obj_id = self.__id_from_path(obj['Path'])
        parent_id = self.__id_from_path(obj['Parent'])
        if self.__config.has_option(parent_id, obj_id):
            status = _DscStore.ITEM_UPDATE
        else:
            status = _DscStore.ITEM_NEW

        self.__sync_item(obj, obj_id, parent_id, status, True)

class DscController(object):
    CONFIG_PATH = os.environ['HOME'] + '/.config/download-sync-controller.conf'
    SUID_OPTION = 'system_update_id'
    SRT_OPTION = 'service_reset_token'
    DATA_PATH_SECTION = '__data_path__'
    DATA_PATH_OPTION = 'path'

    def __init__(self, rel_path = None):
        
        self.__upnp = _DscUpnp()

        self.__config = ConfigParser.ConfigParser()
        self.__config.read(DscController.CONFIG_PATH)
        
        if rel_path:
            self.__set_data_path(rel_path)
        elif not self.__config.has_section(DscController.DATA_PATH_SECTION):
            self.__set_data_path('download-sync-controller')
            
        self.__store_path = self.__config.get(DscController.DATA_PATH_SECTION,
                                                DscController.DATA_PATH_OPTION)

    def __write_config(self):
        with open(DscController.CONFIG_PATH, 'wb') as configfile:
            self.__config.write(configfile)

    def __set_data_path(self, rel_path):
        data_path = os.environ['HOME'] + '/' + rel_path

        if not self.__config.has_section(DscController.DATA_PATH_SECTION):
            self.__config.add_section(DscController.DATA_PATH_SECTION)

        self.__config.set(DscController.DATA_PATH_SECTION,
                                    DscController.DATA_PATH_OPTION, data_path)

        self.__write_config()

    def __need_sync(self, servers):
        for item in servers:
            device = Device(item)
            uuid = device.get_prop('UDN')
            new_srt = device.get_prop('ServiceResetToken')
            new_id = device.get_prop('SystemUpdateID')

            if self.__config.has_section(uuid):
                cur_id = self.__config.getint(uuid, DscController.SUID_OPTION)
                cur_srt = self.__config.get(uuid, DscController.SRT_OPTION)
                if cur_id == -1 or cur_srt != new_srt:
                    print
                    print u'Server {0} needs *full* sync:'.format(uuid)
                    yield item, uuid, 0, new_id, True
                elif cur_id < new_id:
                    print
                    print u'Server {0} needs sync:'.format(uuid)
                    yield item, uuid, cur_id, new_id, False

    def __check_trackable(self, server):
        try:
            try:
                srt = server.get_prop('ServiceResetToken')
            except:
                raise DscError("'ServiceResetToken' variable not supported")

            try:
                dlna_caps = server.get_prop('DLNACaps')
                if not 'content-synchronization' in dlna_caps:
                    raise
            except:
                raise DscError("'content-synchronization' cap not supported")

            try:
                search_caps = server.get_prop('SearchCaps')
                if not [x for x in search_caps if 'ObjectUpdateID' in x]:
                    raise
                if not [x for x in search_caps if 'ContainerUpdateID' in x]:
                    raise
            except:
                raise DscError("'objectUpdateID' search cap not supported")

            return srt
        except DscError as err:
            print err
            return None
    
    def track(self, server_path, track = True):
        server = Device(server_path)
        server_uuid = server.get_prop('UDN')
        
        if track and not self.__config.has_section(server_uuid):
            srt = self.__check_trackable(server)
            if srt != None:
                self.__config.add_section(server_uuid)

                self.__config.set(server_uuid, DscController.SUID_OPTION, '-1')
                self.__config.set(server_uuid, DscController.SRT_OPTION, srt)
                
                self.__write_config()
            else:
                print u"Sorry, the server {0} has no such capability and " \
                        "will not be tracked.".format(server_path)

        elif not track and self.__config.has_section(server_uuid):
            self.__config.remove_section(server_uuid)
            self.__write_config()
            
            store = _DscStore(self.__store_path, server_uuid)
            store.reset()

    def track_reset(self, server):
        self.track(server, False)
        self.track(server)

    def servers(self):
        print u'Running servers:'

        for item in self.__upnp.get_servers():
            try:
                server = Container(item)
                try:
                    folder_name = server.get_prop('FriendlyName')
                except Exception:
                    folder_name = server.get_prop('DisplayName')
                device = Device(item)
                dev_uuid = device.get_prop('UDN')
                dev_path = device.get_prop('Path')

                print u'{0:<25}  Tracked({2})  {3}  {1}'.format(folder_name,
                                            dev_path,
                                            self.__config.has_option(dev_uuid,
                                                    DscController.SUID_OPTION),
                                            dev_uuid)

            except dbus.exceptions.DBusException as err:
                print u'Cannot retrieve properties for ' + item
                print str(err).strip()[:-1]

    def tracked_servers(self):
        print u'Tracked servers:'

        for name in self.__config.sections():
            if name != DscController.DATA_PATH_SECTION:
                print u'{0:<30}'.format(name)

    def sync(self):
        print u'Syncing...'

        for item, uuid, cur, new, full_sync in \
                                    self.__need_sync(self.__upnp.get_servers()):
            store = _DscStore(self.__store_path, uuid)

            if full_sync:
                print u'Resetting local contents for server {0}'.format(uuid)

                self.track_reset(item)
                store.reset()

                objects = _DscContainer(item).find_containers()
            else:
                objects = _DscContainer(item).find_updates(cur)
                
            for obj in objects:
                if obj['Type'] == 'container':
                    children = _DscContainer(obj['Path']).find_children()
                    store.sync_container(obj, children)
                else:
                    store.sync_item(obj)

            self.__config.set(uuid, DscController.SUID_OPTION, str(new))
            self.__write_config()

        print
        print u'Done.'

    def reset(self):
        for name in self.__config.sections():
            if name != DscController.DATA_PATH_SECTION:
                self.__config.remove_section(name)

                store = _DscStore(self.__store_path, name)
                store.reset()

        self.__write_config()


if __name__ == '__main__':
    controller = DscController()
    controller.servers()
    print
    print u'"controller" instance is ready for use.'
