aegir-project
=============
TODO
- rename to install-profile-project


A project-abstraktion on top of platforms and sites
* A project represents a development project that is based on an install profile
* The project/profile is build via a stub makefile in a github repository
* The project knows
  - the raw url for the makefile + a username+token
  - default install profile
* A project can contain a number of deployment, each deployment is
  - A checkout of a specific version of the project
  - A site (database + files) installed on the platform

Hostmaster additions
------------
* A "Project" tab that lists all currently defined projects and allows the user to
  define new projects

* A Project view that shows the properties of a project and all current deployments
  of the project

* A deployment view, that allows the user to update deployment to a new version

Flow
----
### Creation of a project
* User clicks create project and enters data
  - An instance of a project is stored with (name, makefile url, basename, profile, ...)
  - (provision is not involved yet)
* User creates a project deployment (name, revision)
  - A row is added to project_deployment
  - A "Deploy Project" task is started (platform create + git info)
  - Provision build the platform and imports it into hostmaster
  - A site is installed with the default profile
  - provision installs the site
* User upgrates an instance
  - A new project_platform is build
  - the site is migrated to the platform
* User deletes a project instance
  - A delete site + delete platform task is queued
  - Upon completion the project-instance row is deleted in the database



Implementation
--------------
A project instance is a site installed on a platform build from a specific version
of the project. A site will normally only contain a single site, it is possible
manually deploy more sites on a platform, but aegir-project will only migrate
the sites it has itself created.

The project type contains


### Hostmaster
Project
- Name
- Makefile URL (including token and username)
- Profile (default?)
- Domain base
- Project type (default to github which will be the only implementation for now)

Deployment (name, revision, platform, site)
- Records in a project_instance table


### Provision
Project
- (nothing, provision has no knowledge of projects)


Deployments
- each platform has it's revision info and project-name added to its alias
- each site is tagged as a part of a deployment

TODO
----

### Stuff that needs figured out
* Dependencies between tasks
* How to generate github raw url?
* How much should be done in hostmaster, how much in provision?
* Does provision need to know anything about the project?

### Stuff that needs done
* Project content type
* Instance content type (or?)
