#!/bin/bash
# Release Helper Script for CRDL
# Creates a new release with proper versioning

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if version argument is provided
if [ -z "$1" ]; then
    echo -e "${RED}Error: Version number required${NC}"
    echo "Usage: $0 <version> [--dry-run]"
    echo "Example: $0 0.0.2"
    echo "         $0 0.1.0 --dry-run"
    exit 1
fi

VERSION="$1"
DRY_RUN=false

if [ "$2" = "--dry-run" ]; then
    DRY_RUN=true
    echo -e "${YELLOW}Running in DRY RUN mode - no changes will be made${NC}"
fi

# Validate version format (semantic versioning)
if ! [[ $VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo -e "${RED}Error: Invalid version format${NC}"
    echo "Version must be in format: MAJOR.MINOR.PATCH (e.g., 0.0.2)"
    exit 1
fi

TAG="v$VERSION"

echo -e "${GREEN}Preparing release $TAG${NC}"
echo ""

# Check for uncommitted changes
if [ -n "$(git status --porcelain)" ]; then
    echo -e "${YELLOW}Warning: You have uncommitted changes${NC}"
    git status --short
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Check if tag already exists
if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo -e "${RED}Error: Tag $TAG already exists${NC}"
    exit 1
fi

# Update version in CMakeLists.txt
echo "Updating CMakeLists.txt version..."
if [ "$DRY_RUN" = false ]; then
    sed -i "s/^project(crdl VERSION [0-9]\+\.[0-9]\+\.[0-9]\+/project(crdl VERSION $VERSION/" CMakeLists.txt
    echo -e "${GREEN}✓ Updated CMakeLists.txt${NC}"
else
    echo -e "${YELLOW}[DRY RUN] Would update CMakeLists.txt${NC}"
fi

# Show changes
echo ""
echo "Current version in CMakeLists.txt:"
grep "^project(crdl VERSION" CMakeLists.txt

echo ""
echo -e "${GREEN}Release Summary:${NC}"
echo "  Version: $VERSION"
echo "  Tag: $TAG"
echo "  Branch: $(git branch --show-current)"
echo "  Commit: $(git rev-parse --short HEAD)"
echo ""

if [ "$DRY_RUN" = false ]; then
    read -p "Create release? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Release cancelled"
        exit 1
    fi

    # Commit version bump
    git add CMakeLists.txt
    git commit -m "chore: bump version to $VERSION"

    # Create and push tag
    git tag -a "$TAG" -m "Release $TAG"

    echo -e "${GREEN}✓ Created tag $TAG${NC}"

    # Push to remote
    echo ""
    read -p "Push to remote? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        git push origin $(git branch --show-current)
        git push origin "$TAG"
        echo ""
        echo -e "${GREEN}✓ Pushed to remote${NC}"
        echo ""
        echo -e "${GREEN}GitHub Actions will now build the release packages.${NC}"
        echo -e "Check progress at: https://github.com/TanmoyTheBoT/crdl-cpp/actions"
    fi
else
    echo -e "${YELLOW}[DRY RUN] Would create tag $TAG and push to remote${NC}"
fi

echo ""
echo -e "${GREEN}Done!${NC}"
