/*
 * media-service-upnp
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Mark Ryan <mark.d.ryan@intel.com>
 *
 */

#ifndef MSU_INTERFACE_H__
#define MSU_INTERFACE_H__

#define MSU_INTERFACE_PROPERTIES "org.freedesktop.DBus.Properties"
#define MSU_INTERFACE_MEDIA_CONTAINER "org.gnome.UPnP.MediaContainer2"
#define MSU_INTERFACE_MEDIA_OBJECT "org.gnome.UPnP.MediaObject2"
#define MSU_INTERFACE_MEDIA_ITEM "org.gnome.UPnP.MediaItem2"

/* Object Properties */
#define MSU_INTERFACE_PROP_PATH "Path"
#define MSU_INTERFACE_PROP_PARENT "Parent"
#define MSU_INTERFACE_PROP_RESTRICTED "Restricted"
#define MSU_INTERFACE_PROP_DISPLAY_NAME "DisplayName"
#define MSU_INTERFACE_PROP_TYPE "Type"
#define MSU_INTERFACE_PROP_CREATOR "Creator"
#define MSU_INTERFACE_PROP_DLNA_MANAGED "DLNAManaged"
#define MSU_INTERFACE_PROP_OBJECT_UPDATE_ID "ObjectUpdateID"

/* Item Properties */
#define MSU_INTERFACE_PROP_REFPATH "RefPath"
#define MSU_INTERFACE_PROP_ARTIST "Artist"
#define MSU_INTERFACE_PROP_ARTISTS "Artists"
#define MSU_INTERFACE_PROP_ALBUM "Album"
#define MSU_INTERFACE_PROP_DATE "Date"
#define MSU_INTERFACE_PROP_GENRE "Genre"
#define MSU_INTERFACE_PROP_TRACK_NUMBER "TrackNumber"
#define MSU_INTERFACE_PROP_ALBUM_ART_URL "AlbumArtURL"
#define MSU_INTERFACE_PROP_RESOURCES "Resources"

/* Container Properties */
#define MSU_INTERFACE_PROP_SEARCHABLE "Searchable"
#define MSU_INTERFACE_PROP_CHILD_COUNT "ChildCount"
#define MSU_INTERFACE_PROP_CREATE_CLASSES "CreateClasses"
#define MSU_INTERFACE_PROP_CONTAINER_UPDATE_ID "ContainerUpdateID"
#define MSU_INTERFACE_PROP_TOTAL_DELETED_CHILD_COUNT "TotalDeletedChildCount"

/* Device Properties */
#define MSU_INTERFACE_PROP_LOCATION "Location"
#define MSU_INTERFACE_PROP_UDN "UDN"
#define MSU_INTERFACE_PROP_DEVICE_TYPE "DeviceType"
#define MSU_INTERFACE_PROP_FRIENDLY_NAME "FriendlyName"
#define MSU_INTERFACE_PROP_MANUFACTURER "Manufacturer"
#define MSU_INTERFACE_PROP_MANUFACTURER_URL "ManufacturerUrl"
#define MSU_INTERFACE_PROP_MODEL_DESCRIPTION "ModelDescription"
#define MSU_INTERFACE_PROP_MODEL_NAME "ModelName"
#define MSU_INTERFACE_PROP_MODEL_NUMBER "ModelNumber"
#define MSU_INTERFACE_PROP_MODEL_URL "ModelURL"
#define MSU_INTERFACE_PROP_SERIAL_NUMBER "SerialNumber"
#define MSU_INTERFACE_PROP_PRESENTATION_URL "PresentationURL"
#define MSU_INTERFACE_PROP_ICON_URL "IconURL"
#define MSU_INTERFACE_PROP_SV_DLNA_CAPABILITIES "DLNACaps"
#define MSU_INTERFACE_PROP_SV_SEARCH_CAPABILITIES "SearchCaps"
#define MSU_INTERFACE_PROP_SV_SORT_CAPABILITIES "SortCaps"
#define MSU_INTERFACE_PROP_SV_SORT_EXT_CAPABILITIES "SortExtCaps"
#define MSU_INTERFACE_PROP_SV_FEATURE_LIST "FeatureList"

/* Resources Properties */
#define MSU_INTERFACE_PROP_MIME_TYPE "MIMEType"
#define MSU_INTERFACE_PROP_DLNA_PROFILE "DLNAProfile"
#define MSU_INTERFACE_PROP_SIZE "Size"
#define MSU_INTERFACE_PROP_DURATION "Duration"
#define MSU_INTERFACE_PROP_BITRATE "Bitrate"
#define MSU_INTERFACE_PROP_SAMPLE_RATE "SampleRate"
#define MSU_INTERFACE_PROP_BITS_PER_SAMPLE "BitsPerSample"
#define MSU_INTERFACE_PROP_WIDTH "Width"
#define MSU_INTERFACE_PROP_HEIGHT "Height"
#define MSU_INTERFACE_PROP_COLOR_DEPTH "ColorDepth"
#define MSU_INTERFACE_PROP_URLS "URLs"
#define MSU_INTERFACE_PROP_URL "URL"
#define MSU_INTERFACE_PROP_UPDATE_COUNT "UpdateCount"

/* Evented State Variable Properties */
#define MSU_INTERFACE_PROP_ESV_SYSTEM_UPDATE_ID "SystemUpdateID"
#define MSU_INTERFACE_PROP_ESV_SERVICE_RESET_TOKEN "ServiceResetToken"

#define MSU_INTERFACE_GET_VERSION "GetVersion"
#define MSU_INTERFACE_GET_SERVERS "GetServers"
#define MSU_INTERFACE_RELEASE "Release"
#define MSU_INTERFACE_SET_PROTOCOL_INFO "SetProtocolInfo"
#define MSU_INTERFACE_PREFER_LOCAL_ADDRESSES "PreferLocalAddresses"

#define MSU_INTERFACE_FOUND_SERVER "FoundServer"
#define MSU_INTERFACE_LOST_SERVER "LostServer"

#define MSU_INTERFACE_LIST_CHILDREN "ListChildren"
#define MSU_INTERFACE_LIST_CHILDREN_EX "ListChildrenEx"
#define MSU_INTERFACE_LIST_ITEMS "ListItems"
#define MSU_INTERFACE_LIST_ITEMS_EX "ListItemsEx"
#define MSU_INTERFACE_LIST_CONTAINERS "ListContainers"
#define MSU_INTERFACE_LIST_CONTAINERS_EX "ListContainersEx"
#define MSU_INTERFACE_SEARCH_OBJECTS "SearchObjects"
#define MSU_INTERFACE_SEARCH_OBJECTS_EX "SearchObjectsEx"
#define MSU_INTERFACE_UPDATE "Update"

#define MSU_INTERFACE_GET_COMPATIBLE_RESOURCE "GetCompatibleResource"

#define MSU_INTERFACE_GET "Get"
#define MSU_INTERFACE_GET_ALL "GetAll"
#define MSU_INTERFACE_INTERFACE_NAME "InterfaceName"
#define MSU_INTERFACE_PROPERTY_NAME "PropertyName"
#define MSU_INTERFACE_PROPERTIES_VALUE "Properties"
#define MSU_INTERFACE_VALUE "value"
#define MSU_INTERFACE_CHILD_TYPES "ChildTypes"

#define MSU_INTERFACE_VERSION "Version"
#define MSU_INTERFACE_SERVERS "Servers"

#define MSU_INTERFACE_CRITERIA "Criteria"
#define MSU_INTERFACE_DICT "Dictionary"
#define MSU_INTERFACE_PATH "Path"
#define MSU_INTERFACE_QUERY "Query"
#define MSU_INTERFACE_PROTOCOL_INFO "ProtocolInfo"
#define MSU_INTERFACE_PREFER "Prefer"

#define MSU_INTERFACE_OFFSET "Offset"
#define MSU_INTERFACE_MAX "Max"
#define MSU_INTERFACE_FILTER "Filter"
#define MSU_INTERFACE_CHILDREN "Children"
#define MSU_INTERFACE_SORT_BY "SortBy"
#define MSU_INTERFACE_TOTAL_ITEMS "TotalItems"

#define MSU_INTERFACE_PROPERTIES_CHANGED "PropertiesChanged"
#define MSU_INTERFACE_CHANGED_PROPERTIES "ChangedProperties"
#define MSU_INTERFACE_INVALIDATED_PROPERTIES "InvalidatedProperties"
#define MSU_INTERFACE_CONTAINER_UPDATE "ContainerUpdate"
#define MSU_INTERFACE_CONTAINER_PATHS "ContainerPaths"

#define MSU_INTERFACE_DELETE "Delete"

#define MSU_INTERFACE_CREATE_CONTAINER "CreateContainer"
#define MSU_INTERFACE_CREATE_CONTAINER_IN_ANY "CreateContainerInAnyContainer"

#define MSU_INTERFACE_UPLOAD "Upload"
#define MSU_INTERFACE_UPLOAD_TO_ANY "UploadToAnyContainer"
#define MSU_INTERFACE_GET_UPLOAD_STATUS "GetUploadStatus"
#define MSU_INTERFACE_GET_UPLOAD_IDS "GetUploadIDs"
#define MSU_INTERFACE_CANCEL_UPLOAD "CancelUpload"
#define MSU_INTERFACE_TOTAL "Total"
#define MSU_INTERFACE_LENGTH "Length"
#define MSU_INTERFACE_FILE_PATH "FilePath"
#define MSU_INTERFACE_UPLOAD_ID "UploadId"
#define MSU_INTERFACE_UPLOAD_IDS "UploadIDs"
#define MSU_INTERFACE_UPLOAD_STATUS "UploadStatus"
#define MSU_INTERFACE_UPLOAD_UPDATE "UploadUpdate"
#define MSU_INTERFACE_TO_ADD_UPDATE "ToAddUpdate"
#define MSU_INTERFACE_TO_DELETE "ToDelete"
#define MSU_INTERFACE_CANCEL "Cancel"

#endif

