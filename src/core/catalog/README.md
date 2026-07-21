# Catalog domain

This domain owns the in-memory catalog presented to QML. It accepts entries from
configured JSON feeds and from loaded `ISourcePlugin` instances.

## Main types

- `CatalogFeedLoader` downloads a configured feed.
- `CatalogParser` converts feed JSON into `CatalogEntry` values.
- `CatalogController` selects, merges and refreshes sources.
- `CatalogModel` is the QML list model; `catalog_types` defines its values.
- `CatalogFilterService` owns presentation filtering.
- `GameMetadataService` resolves Steam metadata.
- `CoverImageCache` and `CatalogCoverCoordinator` manage cover acquisition.

The controller reports loading state and events; the façade forwards them to
`Arachnel.Core`. It does not create jobs or install games.

## Boundaries

Catalog records describe remote content. Persistent installed-game state belongs
to `library/`, and download lifecycle belongs to `jobs/` and `install/`.
Plugin-specific catalog acquisition and source behaviour remain in the plugin.

When adding a field, update `CatalogEntry`, parser/model roles and the consumer
that owns the field. Keep network handling and parsing separate.
