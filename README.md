# GIT Project #

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

* A "Project" tab that lists all currently defined projects and allows the user to
  define new projects

* A Project view that shows the properties of a project and all current deployments
  of the project

* A deployment view, that allows the user to update deployment to a new version

Flow
----
### Creation of a project
* User clicks create project and enters data
  - A Project node is created and associated hosting_git_project tabel is updated
    with a row containing the makefile url, basename, profile, ...
  - (provision is not involved yet)
* User navigates to the project and click create deployment
  - User is prompted for deployment info (name)
* User clicks update, specifies revision an clicks execute
  - User is promptet for a new revision specification (sha/branch/tag)
  V2 User is also offerede the possibility to migrate to previous versions
  - A "Deploy Project" task is started (platform create + git info)
  - Provision receivies the Deploy Project task
    o downloads the make-file and injects a revspec
    o saves a platform alias (injecting deployment id)
    o verifies the platform
    o migrates/installs a site (injecting/updating deployment id)

  - Provision build the platform and imports it into hostmaster
  - A site is installed with the default profile
  - provision installs the site
* User upgrates an instance
  - A new project_platform is build
  - the site is migrated to the platform
* User deletes a project instance
  - A delete site + delete platform task is queued
  - Upon completion the project-instance row is deleted in the database



deployment DEV 1
- platform( SHA 1)
- site (db, files )

DEV 2
- platform (SHA 2)


Implementation
--------------
A project instance is a site installed on a platform build from a specific version
of the project. A site will normally only contain a single site, it is possible
manually deploy more sites on a platform, but aegir-project will only migrate
the sites it has itself created.

The project type contains


### Hostmaster
Project
- Name (Administrative name)
- Makefile URL (including token and username)
- Profile (Installation profile to use)
- Domain base (deployements will be deployed under [deployname].[domain base])
- Project type (default to github which will be the only implementation for now)
- DB server reference
- Web server reference

Deployment
- Name
TODO - Domain (optional, normalised name is used if not specified)
- Branch (optional)
- Tag (optional)
- Revision (SHA)
- Platform (points to current platform)
- Site (points to the site containing the deployemnts db and files)
- Status (active, unused, needs-rebuild)


### Provision
Project
- (nothing, provision has no knowledge of projects)


Deployments
- each platform has it's revision info and project-name added to its alias
- each site is tagged as a part of a deployment


PROVISION COMMANDS
------------------


# Create a project #
    drush git-project-create \
            "Scleroseforeningen" \
            "scl.reload.dk" \
            "sclerosis" \
            "https://raw.github.com/reload/Scleroseforeningen/master/build_sclerosis.make?login=danquah&token=34b385e88e61e6492e8238b32b0fa8c1"
  - stores a git alias

# Define deployment #
    drush git-project-deployment-define \
          @git_project_scleroseforeningen \
          Nightly \
          --branch=develop 
  - stores a git alias

# First deployment #
    drush git-project-deploy @git_project_scleroseforeningen nightly
  - downloads revision-specific makefile
  - inject revision info (branch = develop)
  - build platform
  - (first deployment) installes develop site on platform

# Subsequent deployments (update on current branch) #
    drush git-project-deploy @git_project_scleroseforeningen nightly --tag=1.0.1
  - downloads revision-specific makefile
  - inject revision info (tag = 1.0.1)
  - build platform
  - migrate previous site onto platform

